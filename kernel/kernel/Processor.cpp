#include <kernel/CPUID.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Processor.h>
#include <kernel/Scheduler.h>
#include <kernel/Terminal/TerminalDriver.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

#if ARCH(x86_64)
	static constexpr uint32_t MSR_IA32_FS_BASE = 0xC0000100;
	static constexpr uint32_t MSR_IA32_GS_BASE = 0xC0000101;
	static constexpr uint32_t MSR_IA32_KERNEL_GS_BASE = 0xC0000102;

	static constexpr uint32_t MSR_IA32_EFER = 0xC0000080;
	static constexpr uint32_t MSR_IA32_STAR = 0xC0000081;
	static constexpr uint32_t MSR_IA32_LSTAR = 0xC0000082;
	static constexpr uint32_t MSR_IA32_FMASK = 0xC0000084;
#endif

	static constexpr uint32_t MSR_IA32_TSC_AUX = 0xC0000103;

	ProcessorID          Processor::s_bsp_id                     { PROCESSOR_NONE };
	BAN::Atomic<uint8_t> Processor::s_processor_count            { 0 };
	BAN::Atomic<bool>    Processor::s_is_smp_enabled             { false };
	paddr_t              Processor::s_shared_page_paddr          { 0 };
	vaddr_t              Processor::s_shared_page_vaddr          { 0 };

	static BAN::Atomic<uint8_t>  s_processors_created { 0 };

	// 32 bit milli seconds are definitely enough as APs start on boot
	static BAN::Atomic<uint32_t> s_first_ap_ready_ms { static_cast<uint32_t>(-1) };

	static BAN::Array<Processor,   0xFF> s_processors;
	static BAN::Array<ProcessorID, 0xFF> s_processor_ids { PROCESSOR_NONE };

	extern "C" void asm_syscall_handler();
	extern "C" void asm_yield_trampoline(uintptr_t);

	ProcessorID Processor::read_processor_id()
	{
		uint32_t id;
		asm volatile(
			"movl $1, %%eax;"
			"cpuid;"
			"shrl $24, %%ebx;"
			: "=b"(id)
			:: "eax", "ecx", "edx"
		);
		return ProcessorID(id);
	}

	Processor& Processor::create(ProcessorID id)
	{
		// bsp is the first processor
		if (s_bsp_id == PROCESSOR_NONE && id == PROCESSOR_NONE)
			s_bsp_id = id = read_processor_id();
		if (s_bsp_id == PROCESSOR_NONE || id == PROCESSOR_NONE || id.m_id >= s_processors.size())
			Kernel::panic("Trying to initialize invalid processor {}", id.m_id);

		if (id == s_bsp_id)
		{
			for (auto& processor : s_processors)
			{
				processor.m_id = PROCESSOR_NONE;
				processor.m_index = 0xFF;
			}
		}

		auto& processor = s_processors[id.m_id];

		ASSERT(processor.m_id == PROCESSOR_NONE);
		processor.m_id = id;

		processor.m_gdt = GDT::create(&processor);
		ASSERT(processor.m_gdt);

		processor.m_idt = IDT::create();
		ASSERT(processor.m_idt);

		processor.m_scheduler = MUST(Scheduler::create());
		ASSERT(processor.m_scheduler);

		s_processors_created++;

		return processor;
	}

	Processor& Processor::initialize()
	{
		auto id = read_processor_id();
		auto& processor = s_processors[id.m_id];

		ASSERT(processor.m_gdt);
		processor.m_gdt->load();

		// initialize GS
#if ARCH(x86_64)
		{
			// set gs base to pointer to this processor
			const uint64_t val = reinterpret_cast<uint64_t>(&processor);
			const uint32_t val_hi = val >> 32;
			const uint32_t val_lo = val & 0xFFFFFFFF;
			asm volatile("wrmsr" :: "d"(val_hi), "a"(val_lo), "c"(MSR_IA32_GS_BASE));
		}
#elif ARCH(i686)
		asm volatile("movw %0, %%gs" :: "r"(static_cast<uint16_t>(0x28)));
#endif

#if ARCH(x86_64)
		// enable syscall instruction
		asm volatile("rdmsr; orb $1, %%al; wrmsr" :: "c"(MSR_IA32_EFER) : "eax", "edx");

		{
			union STAR
			{
				struct
				{
					uint32_t : 32;
					uint16_t sel_ring0;
					uint16_t sel_ring3;
				};
				uint64_t raw;
			};

			// set kernel and user segments
			const uint64_t val = STAR { .sel_ring0 = 0x08, .sel_ring3 = 0x18 | 3 }.raw;
			const uint32_t val_hi = val >> 32;
			const uint32_t val_lo = val & 0xFFFFFFFF;
			asm volatile("wrmsr" :: "d"(val_hi), "a"(val_lo), "c"(MSR_IA32_STAR));
		}
		{
			// set syscall handler address
			const uint64_t val = reinterpret_cast<uint64_t>(&asm_syscall_handler);
			const uint32_t val_hi = val >> 32;
			const uint32_t val_lo = val & 0xFFFFFFFF;
			asm volatile("wrmsr" :: "d"(val_hi), "a"(val_lo), "c"(MSR_IA32_LSTAR));
		}
		{
			// mask DF and IF
			const uint64_t val = (1 << 10) | (1 << 9);
			const uint32_t val_hi = val >> 32;
			const uint32_t val_lo = val & 0xFFFFFFFF;
			asm volatile("wrmsr" :: "d"(val_hi), "a"(val_lo), "c"(MSR_IA32_FMASK));
		}
#endif

		ASSERT(processor.m_idt);
		processor.idt().load();

		disable_sse();

		return processor;
	}

	// NOTE: I don't like this being a separate function but we need heap and page tables for this :)
	void Processor::allocate_stack()
	{
		ASSERT(m_stack_paddr == 0);
		ASSERT(m_stack_vaddr == 0);

		m_stack_paddr = Heap::get().take_free_page();
		ASSERT(m_stack_paddr);

		m_stack_vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
		ASSERT(m_stack_vaddr);

		PageTable::kernel().map_page_at(m_stack_paddr, m_stack_vaddr, PageTable::ReadWrite | PageTable::Present);
	}

	void Processor::initialize_smp()
	{
		const auto processor_id = current_id();
		auto& processor = s_processors[processor_id.as_u32()];

		const paddr_t smp_paddr = Heap::get().take_free_page();
		ASSERT(smp_paddr);

		const vaddr_t smp_vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
		ASSERT(smp_vaddr);

		PageTable::kernel().map_page_at(
			smp_paddr, smp_vaddr,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			PageTable::MemoryType::Uncached
		);

		auto* smp_storage = reinterpret_cast<SMPMessage*>(smp_vaddr);

		constexpr size_t smp_storage_entries = PAGE_SIZE / sizeof(SMPMessage);
		for (size_t i = 0; i < smp_storage_entries - 1; i++)
			smp_storage[i].next = &smp_storage[i + 1];
		smp_storage[smp_storage_entries - 1].next = nullptr;

		processor.m_smp_pending = nullptr;
		processor.m_smp_free    = smp_storage;
	}

	void Processor::initialize_shared_page()
	{
		[[maybe_unused]] constexpr size_t max_processors = (PAGE_SIZE - sizeof(API::SharedPage)) / sizeof(decltype(*API::SharedPage::cpus));
		ASSERT(s_processors_created < max_processors);

		s_shared_page_paddr = Heap::get().take_free_page();
		ASSERT(s_shared_page_paddr);

		s_shared_page_vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
		ASSERT(s_shared_page_vaddr);

		PageTable::kernel().map_page_at(
			s_shared_page_paddr,
			s_shared_page_vaddr,
			PageTable::ReadWrite | PageTable::Present
		);

		memset(reinterpret_cast<void*>(s_shared_page_vaddr), 0, PAGE_SIZE);

		auto& shared_page = *reinterpret_cast<volatile API::SharedPage*>(s_shared_page_vaddr);
		shared_page.gdt_cpu_offset = GDT::cpu_index_offset();
		shared_page.features = 0;

		if (CPUID::has_rdtscp())
			shared_page.features |= API::SPF_RDTSCP;

		ASSERT(Processor::count() + sizeof(Kernel::API::SharedPage) <= PAGE_SIZE);
	}

	ProcessorID Processor::id_from_index(size_t index)
	{
		ASSERT(index < s_processor_count);
		ASSERT(s_processor_ids[index] != PROCESSOR_NONE);
		return s_processor_ids[index];
	}

	void Processor::wait_until_processors_ready()
	{
		initialize_smp();

		// wait until bsp is ready
		if (current_is_bsp())
		{
			initialize_shared_page();

			s_processor_count = 1;
			s_processor_ids[0] = current_id();
			s_processors[current_id().as_u32()].m_index = 0;

			// single processor system
			if (s_processors_created == 1)
				return;

			// wait until first AP is ready
			const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 1000;
			while (s_first_ap_ready_ms == static_cast<uint32_t>(-1))
			{
				if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				{
					dprintln("Could not initialize any APs :(");
					return;
				}
				__builtin_ia32_pause();
			}
		}
		else
		{
			// wait until bsp is ready, it shall get index 0
			while (s_processor_count == 0)
				__builtin_ia32_pause();

			const auto index = s_processor_count++;
			ASSERT(s_processor_ids[index] == PROCESSOR_NONE);
			s_processor_ids[index] = current_id();
			s_processors[current_id().as_u32()].m_index = index;

			uint32_t expected = static_cast<uint32_t>(-1);
			s_first_ap_ready_ms.compare_exchange(expected, SystemTimer::get().ms_since_boot());
		}

		// wait until all processors are initialized
		{
			const uint32_t timeout_ms = s_first_ap_ready_ms + 1000;
			while (s_processor_count < s_processors_created)
			{
				if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				{
					if (current_is_bsp())
						dprintln("Could not initialize {} processors :(", s_processors_created - s_processor_count);
					break;
				}
				__builtin_ia32_pause();
			}
		}
	}

	void Processor::initialize_tsc(uint64_t realtime_seconds)
	{
		auto& shared_page = Processor::shared_page();
		shared_page.gettime_shared.realtime_s = realtime_seconds;
		shared_page.gettime_shared.realtime_ns = 0;

		update_tsc();

		broadcast_smp_message({
			.type = SMPMessage::Type::UpdateTSC,
			.dummy = 0,
		});

		bool everyone_initialized { false };
		while (!everyone_initialized)
		{
			everyone_initialized = true;
			for (size_t i = 0; i < count(); i++)
			{
				if (shared_page.cpus[i].gettime_local.seq != 0)
					continue;
				everyone_initialized = false;
				break;
			}
		}

		shared_page.features |= API::SPF_GETTIME;
	}

	void Processor::update_tsc()
	{
		auto& lgettime = shared_page().cpus[current_index()].gettime_local;

		const auto seq = BAN::atomic_load(lgettime.seq, BAN::memory_order_relaxed);

		BAN::atomic_store(lgettime.seq, seq + 1, BAN::memory_order_release);

		if (lgettime.seq == 1)
		{
			const auto tsc_info = SystemTimer::get().tsc_info();
			lgettime.shift = tsc_info.shift;
			lgettime.mult  = tsc_info.mult;
			lgettime.last_ns  = SystemTimer::get().ns_since_boot_no_tsc();
			lgettime.last_tsc = __builtin_ia32_rdtsc();

			if (CPUID::has_rdtscp())
				asm volatile("wrmsr" :: "d"(0x00000000), "a"(current_index()), "c"(MSR_IA32_TSC_AUX));
		}
		else
		{
			const auto current_ns = SystemTimer::get().ns_since_boot_no_tsc();
			const auto current_tsc = __builtin_ia32_rdtsc();

			auto delta_ns = current_tsc - lgettime.last_tsc;
			if (lgettime.shift >= 0)
				delta_ns <<= lgettime.shift;
			else
				delta_ns >>= -lgettime.shift;
			delta_ns = (delta_ns * lgettime.mult) >> 32;

			lgettime.last_ns += delta_ns;
			lgettime.last_tsc = current_tsc;

			// scale mult by [-0.25%, 0.25%] to fix for clock drift
			const auto error_ns = static_cast<int64_t>(current_ns) - static_cast<int64_t>(lgettime.last_ns);
			const auto correction_ppm = BAN::Math::clamp<int64_t>(error_ns * 1'000'000 / 1'000'000'000, -100, 100);
			const auto correction_delta = -lgettime.mult * correction_ppm / 1'000'000;
			lgettime.mult += correction_delta;
		}

		BAN::atomic_store(lgettime.seq, seq + 2, BAN::memory_order_release);
	}

	uint64_t Processor::ns_since_boot_tsc()
	{
		const auto& shared_page = Processor::shared_page();
		const auto& lgettime = shared_page.cpus[current_index()].gettime_local;

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		uint64_t current_ns = __builtin_ia32_rdtsc() - lgettime.last_tsc;
		if (lgettime.shift >= 0)
			current_ns <<= lgettime.shift;
		else
			current_ns >>= -lgettime.shift;
		current_ns = (current_ns * lgettime.mult) >> 32;
		current_ns += lgettime.last_ns;

		set_interrupt_state(state);

		return current_ns;
	}

	void Processor::handle_ipi()
	{
		handle_smp_messages();
	}

	void Processor::load_segments()
	{
		load_fsbase();
		load_gsbase();
	}

	void Processor::load_fsbase()
	{
		const auto addr = scheduler().current_thread().get_fsbase();
#if ARCH(x86_64)
		const uint32_t addr_hi = addr >> 32;
		const uint32_t addr_lo = addr & 0xFFFFFFFF;
		asm volatile("wrmsr" :: "d"(addr_hi), "a"(addr_lo), "c"(MSR_IA32_FS_BASE));
#elif ARCH(i686)
		gdt().set_fsbase(addr);
#endif
	}

	void Processor::load_gsbase()
	{
		const auto addr = scheduler().current_thread().get_gsbase();
#if ARCH(x86_64)
		const uint32_t addr_hi = addr >> 32;
		const uint32_t addr_lo = addr & 0xFFFFFFFF;
		asm volatile("wrmsr" :: "d"(addr_hi), "a"(addr_lo), "c"(MSR_IA32_KERNEL_GS_BASE));
#elif ARCH(i686)
		gdt().set_gsbase(addr);
#endif
	}

	void Processor::lock_tlb_lock()
	{
		bool expected = false;
		while (!m_tlb_lock.compare_exchange(expected, true, BAN::MemoryOrder::memory_order_acquire))
		{
			__builtin_ia32_pause();
			expected = false;
		}
	}

	void Processor::unlock_tlb_lock()
	{
		m_tlb_lock.store(false, BAN::MemoryOrder::memory_order_release);
	}

	void Processor::handle_smp_messages()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto processor_id = current_id();
		auto& processor = s_processors[processor_id.m_id];

		ASSERT(!processor.m_smp_messages_disabled);

		auto* pending = processor.m_smp_pending.exchange(nullptr);
		if (pending == nullptr)
			return set_interrupt_state(state);

		// reverse smp message queue from LIFO to FIFO
		{
			SMPMessage* reversed = nullptr;

			for (auto* message = pending; message;)
			{
				SMPMessage* next = message->next;
				message->next = reversed;
				reversed = message;
				message = next;
			}

			pending = reversed;
		}

		SMPMessage* last_handled = nullptr;

		// handle messages
		for (auto* message = pending; message; message = message->next)
		{
			switch (message->type)
			{
				case SMPMessage::Type::FlushTLB:
					ASSERT_NOT_REACHED();
				case SMPMessage::Type::NewThread:
					processor.m_scheduler->add_thread(message->new_thread);
					break;
				case SMPMessage::Type::UnblockThread:
					processor.m_scheduler->unblock_thread(message->unblock_thread);
					break;
				case SMPMessage::Type::UpdateTSC:
					update_tsc();
					break;
#if WITH_PROFILING
				case SMPMessage::Type::StartProfiling:
					processor.start_profiling();
					break;
#endif
				case SMPMessage::Type::StackTrace:
					dwarnln("Stack trace of CPU {}", current_id().as_u32());
					Debug::dump_stack_trace();
					break;
			}

			last_handled = message;
		}

		last_handled->next = processor.m_smp_free;
		while (!processor.m_smp_free.compare_exchange(last_handled->next, pending))
		{
			__builtin_ia32_pause();
			last_handled->next = processor.m_smp_free;
		}

		{
			processor.lock_tlb_lock();
			const size_t tlb_entry_count = processor.m_tlb_entry_count;
			const auto tlb_entries = processor.m_tlb_entries;
			const bool tlb_global = processor.m_tlb_global;
			processor.m_tlb_entry_count = 0;
			processor.m_tlb_global = false;
			processor.unlock_tlb_lock();

			auto& page_table = PageTable::current();

			size_t pages = 0;
			for (size_t i = 0; i < tlb_entry_count; i++)
				if (tlb_entries[i].page_table == nullptr || tlb_entries[i].page_table == &page_table)
					pages += tlb_entries[i].page_count;

			if (pages >= PageTable::full_tlb_flush_threshold || tlb_entry_count >= processor.m_tlb_entries.size())
				page_table.invalidate_full_address_space(tlb_global);
			else for (size_t i = 0; i < tlb_entry_count; i++)
				if (tlb_entries[i].page_table == nullptr || tlb_entries[i].page_table == &page_table)
					page_table.invalidate_range(tlb_entries[i].vaddr, tlb_entries[i].page_count, false);
		}

		set_interrupt_state(state);
	}

	bool Processor::send_smp_message(ProcessorID processor_id, const SMPMessage& message, bool send_ipi)
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto& processor = s_processors[processor_id.m_id];

		if (message.type == SMPMessage::Type::FlushTLB)
		{
			processor.lock_tlb_lock();

			const bool is_first_entry = (processor.m_tlb_entry_count == 0);

			const auto& tlb_msg = message.flush_tlb;

			processor.m_tlb_global |= (tlb_msg.page_table == nullptr);

			if (processor.m_tlb_entry_count < processor.m_tlb_entries.size())
			{
				processor.m_tlb_entries[processor.m_tlb_entry_count++] = {
					.vaddr = tlb_msg.vaddr,
					.page_count = tlb_msg.page_count,
					.page_table = static_cast<PageTable*>(tlb_msg.page_table),
				};
			}

			processor.unlock_tlb_lock();
			set_interrupt_state(state);

			return is_first_entry;
		}

		// find a slot for message
		auto* storage = processor.m_smp_free.exchange(nullptr);
		while (storage == nullptr)
		{
			Processor::pause();
			storage = processor.m_smp_free.exchange(nullptr);
		}

		if (auto* base = storage->next)
		{
			SMPMessage* null = nullptr;
			if (!processor.m_smp_free.compare_exchange(null, base))
			{
				// NOTE: this is an annoying traversal, but most of the time
				//       above if condition bypasses this :)
				auto* last = base;
				while (last->next)
					last = last->next;

				last->next = processor.m_smp_free;
				while (!processor.m_smp_free.compare_exchange(last->next, base))
				{
					__builtin_ia32_pause();
					last->next = processor.m_smp_free;
				}
			}
		}

		// write message
		*storage = message;

		// push message to pending queue
		storage->next = processor.m_smp_pending;
		while (!processor.m_smp_pending.compare_exchange(storage->next, storage))
		{
			__builtin_ia32_pause();
			storage->next = processor.m_smp_pending;
		}

		const bool needs_ipi = (storage->next == nullptr);

		if (send_ipi)
		{
			if (processor_id == current_id())
				handle_smp_messages();
			else if (needs_ipi)
				InterruptController::get().send_ipi(processor_id);
		}

		set_interrupt_state(state);

		return needs_ipi;
	}

	void Processor::broadcast_smp_message(const SMPMessage& message)
	{
		if (!is_smp_enabled())
			return;

		const auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		bool needs_ipi = false;

		const auto current_id = Processor::current_id();
		for (size_t i = 0; i < Processor::count(); i++)
		{
			const auto processor_id = s_processor_ids[i];
			if (processor_id != current_id)
				needs_ipi |= send_smp_message(processor_id, message, false);
		}

		if (needs_ipi)
			InterruptController::get().broadcast_ipi();

		set_interrupt_state(state);
	}

	Processor::LoadStats Processor::get_load_stats(size_t index)
	{
		ASSERT(index < Processor::count());

		auto& processor = s_processors[s_processor_ids[index].as_u32()];

		bool expected = false;
		while (!processor.m_load_stat_lock.compare_exchange(expected, true))
		{
			Processor::pause();
			expected = false;
		}

		const auto load_stats = processor.m_load_stats;

		processor.m_load_stat_lock.store(false);

		return load_stats;
	}

	void Processor::yield()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		ASSERT(!Thread::current().has_spinlock());

		auto& processor = s_processors[current_id().as_u32()];

		{
			bool expected = false;
			while (!processor.m_load_stat_lock.compare_exchange(expected, true))
			{
				Processor::pause();
				expected = false;
			}

			const uint64_t elapsed_ns = SystemTimer::get().ns_since_boot() - processor.m_load_start_ns;

			auto& load_stats = processor.m_load_stats;
			if (scheduler().is_idle())
				load_stats.ns_idle += elapsed_ns;
			load_stats.ns_total += elapsed_ns;

			processor.m_load_stat_lock.store(false);
		}

		if (!scheduler().is_idle())
			Thread::current().set_cpu_time_stop();

		asm_yield_trampoline(processor.stack_top_vaddr());

		processor.m_load_start_ns = SystemTimer::get().ns_since_boot();

		if (!scheduler().is_idle())
			Thread::current().set_cpu_time_start();

		Processor::set_interrupt_state(state);
	}

}
