#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Processor.h>
#include <kernel/Terminal/TerminalDriver.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

extern Kernel::TerminalDriver* g_terminal_driver;

namespace Kernel
{

#if ARCH(x86_64)
	static constexpr uint32_t MSR_IA32_FS_BASE = 0xC0000100;
	static constexpr uint32_t MSR_IA32_GS_BASE = 0xC0000101;
#endif

	ProcessorID          Processor::s_bsb_id                { PROCESSOR_NONE };
	BAN::Atomic<uint8_t> Processor::s_processor_count       { 0 };
	BAN::Atomic<bool>    Processor::s_is_smp_enabled        { false };
	BAN::Atomic<bool>    Processor::s_should_print_cpu_load { false };

	static BAN::Atomic<uint8_t>  s_processors_created { 0 };

	// 32 bit milli seconds are definitely enough as APs start on boot
	static BAN::Atomic<uint32_t> s_first_ap_ready_ms { static_cast<uint32_t>(-1) };

	static BAN::Array<Processor,   0xFF> s_processors;
	static BAN::Array<ProcessorID, 0xFF> s_processor_ids { PROCESSOR_NONE };

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
		// bsb is the first processor
		if (s_bsb_id == PROCESSOR_NONE && id == PROCESSOR_NONE)
			s_bsb_id = id = read_processor_id();
		if (s_bsb_id == PROCESSOR_NONE || id == PROCESSOR_NONE || id.m_id >= s_processors.size())
			Kernel::panic("Trying to initialize invalid processor {}", id.m_id);

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

		SMPMessage* smp_storage = new SMPMessage[0x1000];
		ASSERT(smp_storage);
		for (size_t i = 0; i < 0xFFF; i++)
			smp_storage[i].next = &smp_storage[i + 1];
		smp_storage[0xFFF].next = nullptr;

		processor.m_smp_pending = nullptr;
		processor.m_smp_free    = smp_storage;

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
		// set gs base to pointer to this processor
		uint64_t ptr = reinterpret_cast<uint64_t>(&processor);
		uint32_t ptr_hi = ptr >> 32;
		uint32_t ptr_lo = ptr & 0xFFFFFFFF;
		asm volatile("wrmsr" :: "d"(ptr_hi), "a"(ptr_lo), "c"(MSR_IA32_GS_BASE));
#elif ARCH(i686)
		asm volatile("movw $0x28, %%ax; movw %%ax, %%gs" ::: "ax");
#endif

		ASSERT(processor.m_idt);
		processor.idt().load();

		return processor;
	}

	ProcessorID Processor::id_from_index(size_t index)
	{
		ASSERT(index < s_processor_count);
		ASSERT(s_processor_ids[index] != PROCESSOR_NONE);
		return s_processor_ids[index];
	}

	void Processor::wait_until_processors_ready()
	{
		if (s_processors_created == 1)
		{
			ASSERT(current_is_bsb());
			s_processor_count++;
			s_processor_ids[0] = current_id();
		}

		// wait until bsb is ready
		if (current_is_bsb())
		{
			s_processor_count = 1;
			s_processor_ids[0] = current_id();

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
			// wait until bsb is ready, it shall get index 0
			while (s_processor_count == 0)
				__builtin_ia32_pause();

			auto lookup_index = s_processor_count++;
			ASSERT(s_processor_ids[lookup_index] == PROCESSOR_NONE);
			s_processor_ids[lookup_index] = current_id();

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
					if (current_is_bsb())
						dprintln("Could not initialize {} processors :(", s_processors_created - s_processor_count);
					break;
				}
				__builtin_ia32_pause();
			}
		}

		s_is_smp_enabled = true;
	}

	void Processor::handle_ipi()
	{
		handle_smp_messages();
	}

	template<typename F>
	void with_atomic_lock(BAN::Atomic<bool>& lock, F callback)
	{
		bool expected = false;
		while (!lock.compare_exchange(expected, true, BAN::MemoryOrder::memory_order_acquire))
		{
			__builtin_ia32_pause();
			expected = false;
		}

		callback();

		lock.store(false, BAN::MemoryOrder::memory_order_release);
	}

	void Processor::handle_smp_messages()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto processor_id = current_id();
		auto& processor = s_processors[processor_id.m_id];

		SMPMessage* pending = nullptr;
		with_atomic_lock(processor.m_smp_pending_lock,
			[&]()
			{
				pending = processor.m_smp_pending;
				processor.m_smp_pending = nullptr;
			}
		);

		if (pending)
		{
			// reverse smp message queue from LIFO to FIFO
			{
				SMPMessage* reversed = nullptr;

				for (SMPMessage* message = pending; message;)
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
				}

				last_handled = message;
			}

			with_atomic_lock(processor.m_smp_free_lock,
				[&]()
				{
					last_handled->next = processor.m_smp_free;
					processor.m_smp_free = pending;
				}
			);
		}

		set_interrupt_state(state);
	}

	void Processor::load_tls()
	{
		const auto addr = scheduler().current_thread().get_tls();
#if ARCH(x86_64)
		uint32_t ptr_hi = addr >> 32;
		uint32_t ptr_lo = addr & 0xFFFFFFFF;
		asm volatile("wrmsr" :: "d"(ptr_hi), "a"(ptr_lo), "c"(MSR_IA32_FS_BASE));
#elif ARCH(i686)
		gdt().set_tls(addr);
#endif
	}

	void Processor::send_smp_message(ProcessorID processor_id, const SMPMessage& message, bool send_ipi)
	{
		ASSERT(processor_id != current_id());

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		auto& processor = s_processors[processor_id.m_id];

		// take free message slot
		SMPMessage* storage = nullptr;
		with_atomic_lock(processor.m_smp_free_lock,
			[&]()
			{
				storage = processor.m_smp_free;
				ASSERT(storage && storage->next);

				processor.m_smp_free = storage->next;
			}
		);

		// write message
		*storage = message;

		// push message to pending queue
		with_atomic_lock(processor.m_smp_pending_lock,
			[&]()
			{
				storage->next = processor.m_smp_pending;
				processor.m_smp_pending = storage;
			}
		);

		if (send_ipi)
			InterruptController::get().send_ipi(processor_id);

		set_interrupt_state(state);
	}

	void Processor::broadcast_smp_message(const SMPMessage& message)
	{
		if (!is_smp_enabled())
			return;

		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

		for (size_t i = 0; i < Processor::count(); i++)
		{
			auto processor_id = s_processor_ids[i];
			if (processor_id != current_id())
				send_smp_message(processor_id, message, false);
		}

		InterruptController::get().broadcast_ipi();

		set_interrupt_state(state);
	}

	void Processor::yield()
	{
		auto state = get_interrupt_state();
		set_interrupt_state(InterruptState::Disabled);

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
					const uint64_t load_x1000  = 100'000 * (duration_ns - processor_info.m_idle_ns) / duration_ns;

					uint32_t x = g_terminal_driver->width() - 16;
					uint32_t y = current_id().as_u32();
					const auto proc_putc =
						[&x, y](char ch)
						{
							if (x < g_terminal_driver->width() && y < g_terminal_driver->height())
								g_terminal_driver->putchar_at(ch, x++, y, TerminalColor::BRIGHT_WHITE, TerminalColor::BLACK);
						};

					BAN::Formatter::print(proc_putc, "CPU { 2}: { 3}.{3}%", current_id(), load_x1000 / 1000, load_x1000 % 1000);
				}

				processor_info.m_idle_ns         = 0;
				processor_info.m_last_update_ns  = current_ns;
				processor_info.m_next_update_ns += load_update_interval_ns;
			}
		}

#if ARCH(x86_64)
		asm volatile(
			"movq %%rsp, %%rcx;"
			"movq %[load_sp], %%rsp;"
			"int %[yield];"
			"movq %%rcx, %%rsp;"
			// NOTE: This is offset by 2 pointers since interrupt without PL change
			//       does not push SP and SS. This allows accessing "whole" interrupt stack.
			:: [load_sp]"r"(Processor::current_stack_top() - 2 * sizeof(uintptr_t)),
			   [yield]"i"(IRQ_YIELD)
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
			   [yield]"i"(IRQ_YIELD)
			:  "memory", "ecx"
		);
#else
		#error
#endif

		processor_info.m_start_ns = SystemTimer::get().ns_since_boot();

		Processor::set_interrupt_state(state);
	}

}
