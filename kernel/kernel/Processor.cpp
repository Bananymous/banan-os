#include <kernel/InterruptController.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Processor.h>
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

	ProcessorID          Processor::s_bsp_id                     { PROCESSOR_NONE };
	BAN::Atomic<uint8_t> Processor::s_processor_count            { 0 };
	BAN::Atomic<bool>    Processor::s_is_smp_enabled             { false };
	BAN::Atomic<bool>    Processor::s_should_print_cpu_load      { false };
	paddr_t              Processor::s_shared_page_paddr          { 0 };
	vaddr_t              Processor::s_shared_page_vaddr          { 0 };

	static BAN::Atomic<uint8_t>  s_processors_created { 0 };

	// 32 bit milli seconds are definitely enough as APs start on boot
	static BAN::Atomic<uint32_t> s_first_ap_ready_ms { static_cast<uint32_t>(-1) };

	static BAN::Array<Processor,   0xFF> s_processors;
	static BAN::Array<ProcessorID, 0xFF> s_processor_ids { PROCESSOR_NONE };

	extern "C" void asm_syscall_handler();

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
			for (auto& processor : s_processors)
				processor.m_id = PROCESSOR_NONE;

		auto& processor = s_processors[id.m_id];

		ASSERT(processor.m_id == PROCESSOR_NONE);
		processor.m_id = id;

		processor.m_stack = kmalloc(s_stack_size, 4096, true);
		ASSERT(processor.m_stack);

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
		asm volatile("movw %0, %%gs" :: "r"(0x28));
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

		return processor;
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
		for (size_t i = 0; i <= 0xFF; i++)
			shared_page.__sequence[i] = i;
		shared_page.features = 0;

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

	void Processor::initialize_tsc(uint8_t shift, uint64_t mult, uint64_t realtime_seconds)
	{
		auto& shared_page = Processor::shared_page();

		shared_page.gettime_shared.shift = shift;
		shared_page.gettime_shared.mult = mult;
		shared_page.gettime_shared.realtime_seconds = realtime_seconds;

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
		const auto read_tsc =
			[]() -> uint64_t {
				uint32_t high, low;
				asm volatile("lfence; rdtsc" : "=d"(high), "=a"(low));
				return (static_cast<uint64_t>(high) << 32) | low;
			};

		auto& sgettime = shared_page().cpus[current_index()].gettime_local;
		sgettime.seq = sgettime.seq + 1;
		sgettime.last_ns = SystemTimer::get().ns_since_boot_no_tsc();
		sgettime.last_tsc = read_tsc();
		sgettime.seq = sgettime.seq + 1;
	}

	uint64_t Processor::ns_since_boot_tsc()
	{
		const auto read_tsc =
			[]() -> uint64_t {
				uint32_t high, low;
				asm volatile("lfence; rdtsc" : "=d"(high), "=a"(low));
				return (static_cast<uint64_t>(high) << 32) | low;
			};

		const auto& shared_page = Processor::shared_page();
		const auto& sgettime = shared_page.gettime_shared;
		const auto& lgettime = shared_page.cpus[current_index()].gettime_local;

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		const auto current_ns = lgettime.last_ns + (((read_tsc() - lgettime.last_tsc) * sgettime.mult) >> sgettime.shift);

		set_interrupt_state(state);

		return current_ns;
	}

	void Processor::handle_ipi()
	{
		handle_smp_messages();
	}

	void Processor::handle_smp_messages()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto processor_id = current_id();
		auto& processor = s_processors[processor_id.m_id];

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
					for (size_t i = 0; i < message->flush_tlb.page_count; i++)
						asm volatile("invlpg (%0)" :: "r"(message->flush_tlb.vaddr + i * PAGE_SIZE) : "memory");
					break;
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

		set_interrupt_state(state);
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

	void Processor::send_smp_message(ProcessorID processor_id, const SMPMessage& message, bool send_ipi)
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto& processor = s_processors[processor_id.m_id];

		// find a slot for message
		auto* storage = processor.m_smp_free.exchange(nullptr);
		while (storage == nullptr)
		{
			__builtin_ia32_pause();
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

		if (send_ipi)
		{
			if (processor_id == current_id())
				handle_smp_messages();
			else
				InterruptController::get().send_ipi(processor_id);
		}

		set_interrupt_state(state);
	}

	void Processor::broadcast_smp_message(const SMPMessage& message)
	{
		if (!is_smp_enabled())
			return;

		const auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		const auto current_id = Processor::current_id();
		for (size_t i = 0; i < Processor::count(); i++)
		{
			const auto processor_id = s_processor_ids[i];
			if (processor_id != current_id)
				send_smp_message(processor_id, message, false);
		}

		InterruptController::get().broadcast_ipi();

		set_interrupt_state(state);
	}

	void Processor::yield()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		ASSERT(!Thread::current().has_spinlock());

		auto& processor_info = s_processors[current_id().as_u32()];

		{
			constexpr uint64_t load_update_interval_ns = 1'000'000'000;

			const uint64_t current_ns = SystemTimer::get().ns_since_boot();

			if (scheduler().is_idle())
				processor_info.m_idle_ns += current_ns - processor_info.m_start_ns;

			if (current_ns >= processor_info.m_next_update_ns)
			{
				if (s_should_print_cpu_load && g_terminal_driver)
				{
					const uint64_t duration_ns = current_ns - processor_info.m_last_update_ns;
					const uint64_t load_x1000  = 100'000 * (duration_ns - BAN::Math::min(processor_info.m_idle_ns, duration_ns)) / duration_ns;

					uint32_t x = g_terminal_driver->width() - 16;
					uint32_t y = current_id().as_u32();
					const auto proc_putc =
						[&x, y](char ch)
						{
							if (x < g_terminal_driver->width() && y < g_terminal_driver->height())
								g_terminal_driver->putchar_at(ch, x++, y, TerminalColor::WHITE, TerminalColor::BLACK);
						};

					BAN::Formatter::print(proc_putc, "CPU { 2}: { 3}.{3}%", current_id(), load_x1000 / 1000, load_x1000 % 1000);
				}

				processor_info.m_idle_ns         = 0;
				processor_info.m_last_update_ns  = current_ns;
				processor_info.m_next_update_ns += load_update_interval_ns;
			}
		}

		if (!scheduler().is_idle())
			Thread::current().set_cpu_time_stop();

#if ARCH(x86_64)
		asm volatile(
			"movq %%rsp, %%rcx;"
			"movq %[load_sp], %%rsp;"
			"int %[yield];"
			"movq %%rcx, %%rsp;"
			// NOTE: This is offset by 2 pointers since interrupt without PL change
			//       does not push SP and SS. This allows accessing "whole" interrupt stack.
			:: [load_sp]"r"(Processor::current_stack_top() - 2 * sizeof(uintptr_t)),
			   [yield]"i"(static_cast<int>(IRQ_YIELD)) // WTF GCC 15
			:  "memory", "rcx"
		);
#elif ARCH(i686)
		asm volatile(
			"movl %%esp, %%ecx;"
			"movl %[load_sp], %%esp;"
			"int %[yield];"
			"movl %%ecx, %%esp;"
			// NOTE: This is offset by 2 pointers since interrupt without PL change
			//       does not push SP and SS. This allows accessing "whole" interrupt stack.
			:: [load_sp]"r"(Processor::current_stack_top() - 2 * sizeof(uintptr_t)),
			   [yield]"i"(static_cast<int>(IRQ_YIELD)) // WTF GCC 15
			:  "memory", "ecx"
		);
#else
		#error
#endif

		processor_info.m_start_ns = SystemTimer::get().ns_since_boot();

		if (!scheduler().is_idle())
			Thread::current().set_cpu_time_start();

		Processor::set_interrupt_state(state);
	}

}
