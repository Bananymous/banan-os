#include <BAN/Array.h>
#include <BAN/Errors.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Panic.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/PIT.h>

#define ISR_LIST_X X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31)
#define IRQ_LIST_X  X(  0) X(  1) X(  2) X(  3) X(  4) X(  5) X(  6) X(  7) X(  8) X(  9) X( 10) X( 11) X( 12) X( 13) X( 14) X( 15) X( 16) X( 17) X( 18) X( 19) X( 20) X( 21) X( 22) X( 23) X( 24) X( 25) X( 26) X( 27) X( 28) X( 29) X( 30) X( 31) \
					X( 32) X( 33) X( 34) X( 35) X( 36) X( 37) X( 38) X( 39) X( 40) X( 41) X( 42) X( 43) X( 44) X( 45) X( 46) X( 47) X( 48) X( 49) X( 50) X( 51) X( 52) X( 53) X( 54) X( 55) X( 56) X( 57) X( 58) X( 59) X( 60) X( 61) X( 62) X( 63) \
					X( 64) X( 65) X( 66) X( 67) X( 68) X( 69) X( 70) X( 71) X( 72) X( 73) X( 74) X( 75) X( 76) X( 77) X( 78) X( 79) X( 80) X( 81) X( 82) X( 83) X( 84) X( 85) X( 86) X( 87) X( 88) X( 89) X( 90) X( 91) X( 92) X( 93) X( 94) X( 95) \
					X( 96) X( 97) X( 98) X( 99) X(100) X(101) X(102) X(103) X(104) X(105) X(106) X(107) X(108) X(109) X(110) X(111) X(112) X(113) X(114) X(115) X(116) X(117) X(118) X(119) X(120) X(121) X(122) X(123) X(124) X(125) X(126) X(127) \
					X(128) X(129) X(130) X(131) X(132) X(133) X(134) X(135) X(136) X(137) X(138) X(139) X(140) X(141) X(142) X(143) X(144) X(145) X(146) X(147) X(148) X(149) X(150) X(151) X(152) X(153) X(154) X(155) X(156) X(157) X(158) X(159) \
					X(160) X(161) X(162) X(163) X(164) X(165) X(166) X(167) X(168) X(169) X(170) X(171) X(172) X(173) X(174) X(175) X(176) X(177) X(178) X(179) X(180) X(181) X(182) X(183) X(184) X(185) X(186) X(187) X(188) X(189) X(190) X(191) \
					X(192) X(193) X(194) X(195) X(196) X(197) X(198) X(199) X(200) X(201) X(202) X(203) X(204) X(205) X(206) X(207)

static_assert(Kernel::IRQ_SYSCALL == Kernel::IRQ_VECTOR_BASE + 208);

namespace Kernel
{

#if ARCH(x86_64)
	struct Registers
	{
		uint64_t cr4;
		uint64_t cr3;
		uint64_t cr2;
		uint64_t cr0;

		uint64_t r15;
		uint64_t r14;
		uint64_t r13;
		uint64_t r12;
		uint64_t r11;
		uint64_t r10;
		uint64_t r9;
		uint64_t r8;

		uint64_t rdi;
		uint64_t rsi;
		uint64_t rbp;
		uint64_t rbx;
		uint64_t rdx;
		uint64_t rcx;
		uint64_t rax;
	};
#elif ARCH(i686)
	struct Registers
	{
		uint32_t cr4;
		uint32_t cr3;
		uint32_t cr2;
		uint32_t cr0;

		uint32_t edi;
		uint32_t esi;
		uint32_t ebp;
		uint32_t unused;
		uint32_t ebx;
		uint32_t edx;
		uint32_t ecx;
		uint32_t eax;
	};
#endif

#define X(num) 1 +
	static BAN::Array<Interruptable*, IRQ_LIST_X 0> s_interruptables;
#undef X

	enum ISR
	{
		DivisionError,
		Debug,
		NonMaskableInterrupt,
		Breakpoint,
		Overflow,
		BoundRangeException,
		InvalidOpcode,
		DeviceNotAvailable,
		DoubleFault,
		CoprocessorSegmentOverrun,
		InvalidTSS,
		SegmentNotPresent,
		StackSegmentFault,
		GeneralProtectionFault,
		PageFault,
		UnknownException0x0F,
		x87FloatingPointException,
		AlignmentCheck,
		MachineCheck,
		SIMDFloatingPointException,
		VirtualizationException,
		ControlProtectionException,
		UnknownException0x16,
		UnknownException0x17,
		UnknownException0x18,
		UnknownException0x19,
		UnknownException0x1A,
		UnknownException0x1B,
		HypervisorInjectionException,
		VMMCommunicationException,
		SecurityException,
		UnkownException0x1F,
	};

	struct PageFaultError
	{
		union
		{
			uint32_t raw;
			struct
			{
				uint32_t present		: 1;
				uint32_t write			: 1;
				uint32_t userspace		: 1;
				uint32_t reserved_write	: 1;
				uint32_t instruction	: 1;
				uint32_t protection_key	: 1;
				uint32_t shadow_stack	: 1;
				uint32_t reserved1		: 8;
				uint32_t sgx_violation	: 1;
				uint32_t reserved2		: 16;
			};
		};

	};
	static_assert(sizeof(PageFaultError) == 4);

	static const char* isr_exceptions[] =
	{
		"Division Error",
		"Debug",
		"Non-maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"Bound Range Exception",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack-Segment Fault",
		"General Protection Fault",
		"Page Fault",
		"Unknown Exception 0x0F",
		"x87 Floating-Point Exception",
		"Alignment Check",
		"Machine Check",
		"SIMD Floating-Point Exception",
		"Virtualization Exception",
		"Control Protection Exception",
		"Unknown Exception 0x16",
		"Unknown Exception 0x17",
		"Unknown Exception 0x18",
		"Unknown Exception 0x19",
		"Unknown Exception 0x1A",
		"Unknown Exception 0x1B",
		"Hypervisor Injection Exception",
		"VMM Communication Exception",
		"Security Exception",
		"Unkown Exception 0x1F",
	};

	extern "C" void cpp_isr_handler(uint32_t isr, uint32_t error, InterruptStack* interrupt_stack, const Registers* regs)
	{
		if (g_paniced)
		{
			dprintln("Processor {} halted", Processor::current_id());
			if (InterruptController::is_initialized())
				InterruptController::get().broadcast_ipi();
			asm volatile("cli; 1: hlt; jmp 1b");
		}

		const pid_t tid = Thread::current_tid();
		const pid_t pid = (tid && Thread::current().has_process()) ? Process::current().pid() : 0;

		const char* process_name = "";

		if (tid)
		{
			auto& thread = Thread::current();
			thread.save_sse();

			if (isr == ISR::PageFault && Thread::current().is_userspace())
			{
				if (pid)
				{
					PageFaultError page_fault_error;
					page_fault_error.raw = error;

					Processor::set_interrupt_state(InterruptState::Enabled);
					auto result = Process::current().allocate_page_for_demand_paging(regs->cr2, page_fault_error.write, page_fault_error.instruction);
					Processor::set_interrupt_state(InterruptState::Disabled);

					if (!result.is_error() && result.value())
						goto done;

					if (result.is_error())
					{
						dwarnln("Demand paging: {}", result.error());
						Thread::current().handle_signal(SIGKILL);
						goto done;
					}
				}

				// Check if stack is OOB
				if (ARCH(i686) && !GDT::is_user_segment(interrupt_stack->cs))
					; // 32 bit does not push stack pointer when no CPL change happens
				else if (thread.userspace_stack_bottom() < interrupt_stack->sp && interrupt_stack->sp <= thread.userspace_stack_top())
					; // using userspace stack
				else if (thread.kernel_stack_bottom() < interrupt_stack->sp && interrupt_stack->sp <= thread.kernel_stack_top())
					; // using kernel stack
				else
				{
					derrorln("Stack pointer out of bounds!");
					derrorln("rip {H}", interrupt_stack->ip);
					derrorln("rsp {H}, userspace stack {H}->{H}, kernel stack {H}->{H}",
						interrupt_stack->sp,
						thread.userspace_stack_bottom(), thread.userspace_stack_top(),
						thread.kernel_stack_bottom(), thread.kernel_stack_top()
					);
					Thread::current().handle_signal(SIGKILL);
					goto done;
				}
			}
		}

		Debug::s_debug_lock.lock();

		if (PageTable::current().get_page_flags(interrupt_stack->ip & PAGE_ADDR_MASK) & PageTable::Flags::Present)
		{
			auto* machine_code = (const uint8_t*)interrupt_stack->ip;
			dwarnln("While executing: {2H}{2H}{2H}{2H}{2H}{2H}{2H}{2H}",
				machine_code[0],
				machine_code[1],
				machine_code[2],
				machine_code[3],
				machine_code[4],
				machine_code[5],
				machine_code[6],
				machine_code[7]
			);
		}

		if (Thread::current().has_process())
			process_name = Process::current().name();

#if ARCH(x86_64)
		dwarnln(
			"CPU {}: {} (error code: 0x{8H}), pid {}, tid {}: {}\r\n"
			"Register dump\r\n"
			"rax=0x{16H}, rbx=0x{16H}, rcx=0x{16H}, rdx=0x{16H}\r\n"
			"rsp=0x{16H}, rbp=0x{16H}, rdi=0x{16H}, rsi=0x{16H}\r\n"
			"rip=0x{16H}, rflags=0x{16H}\r\n"
			"cr0=0x{16H}, cr2=0x{16H}, cr3=0x{16H}, cr4=0x{16H}",
			Processor::current_id(), isr_exceptions[isr], error, pid, tid, process_name,
			regs->rax, regs->rbx, regs->rcx, regs->rdx,
			interrupt_stack->sp, regs->rbp, regs->rdi, regs->rsi,
			interrupt_stack->ip, interrupt_stack->flags,
			regs->cr0, regs->cr2, regs->cr3, regs->cr4
		);
#elif ARCH(i686)
		dwarnln(
			"CPU {}: {} (error code: 0x{8H}), pid {}, tid {}: {}\r\n"
			"Register dump\r\n"
			"eax=0x{8H}, ebx=0x{8H}, ecx=0x{8H}, edx=0x{8H}\r\n"
			"esp=0x{8H}, ebp=0x{8H}, edi=0x{8H}, esi=0x{8H}\r\n"
			"eip=0x{8H}, eflags=0x{8H}\r\n"
			"cr0=0x{8H}, cr2=0x{8H}, cr3=0x{8H}, cr4=0x{8H}",
			Processor::current_id(), isr_exceptions[isr], error, pid, tid, process_name,
			regs->eax, regs->ebx, regs->ecx, regs->edx,
			interrupt_stack->sp, regs->ebp, regs->edi, regs->esi,
			interrupt_stack->ip, interrupt_stack->flags,
			regs->cr0, regs->cr2, regs->cr3, regs->cr4
		);
#endif
		if (isr == ISR::PageFault)
			PageTable::current().debug_dump();
		Debug::dump_stack_trace();

		Debug::s_debug_lock.unlock(InterruptState::Disabled);

		if (tid && Thread::current().is_userspace())
		{
			// TODO: Confirm and fix the exception to signal mappings

			int signal = 0;
			switch (isr)
			{
			case ISR::DivisionError:
			case ISR::SIMDFloatingPointException:
			case ISR::x87FloatingPointException:
				signal = SIGFPE;
				break;
			case ISR::AlignmentCheck:
				signal = SIGBUS;
				break;
			case ISR::InvalidOpcode:
				signal = SIGILL;
				break;
			case ISR::PageFault:
				signal = SIGSEGV;
				break;
			default:
				dwarnln("Unhandled exception");
				signal = SIGABRT;
				break;
			}

			Thread::current().handle_signal(signal);
		}
		else
		{
			panic("Unhandled exception");
		}

		ASSERT(Thread::current().state() != Thread::State::Terminated);

	done:
		Thread::current().load_sse();
	}

	extern "C" void cpp_yield_handler(InterruptStack* interrupt_stack, InterruptRegisters* interrupt_registers)
	{
		// yield is raised through kernel software interrupt
		ASSERT(!InterruptController::get().is_in_service(IRQ_YIELD - IRQ_VECTOR_BASE));
		ASSERT(!GDT::is_user_segment(interrupt_stack->cs));
		Processor::scheduler().reschedule(interrupt_stack, interrupt_registers);
	}

	extern "C" void cpp_ipi_handler()
	{
		ASSERT(InterruptController::get().is_in_service(IRQ_IPI - IRQ_VECTOR_BASE));
		InterruptController::get().eoi(IRQ_IPI - IRQ_VECTOR_BASE);
		Processor::handle_ipi();
		Processor::scheduler().reschedule_if_idle();
	}

	extern "C" void cpp_timer_handler()
	{
		if (g_paniced)
		{
			dprintln("Processor {} halted", Processor::current_id());
			if (InterruptController::is_initialized())
				InterruptController::get().broadcast_ipi();
			asm volatile("cli; 1: hlt; jmp 1b");
		}

		Thread::current().save_sse();

		ASSERT(InterruptController::get().is_in_service(IRQ_TIMER - IRQ_VECTOR_BASE));
		InterruptController::get().eoi(IRQ_TIMER - IRQ_VECTOR_BASE);

		if (Processor::current_is_bsb())
			Process::update_alarm_queue();

		Processor::scheduler().timer_interrupt();

		auto& current_thread = Thread::current();
		if (current_thread.can_add_signal_to_execute())
			current_thread.handle_signal();

		Thread::current().load_sse();
	}

	extern "C" void cpp_irq_handler(uint32_t irq)
	{
		if (g_paniced)
		{
			while (Debug::s_debug_lock.current_processor_has_lock())
				Debug::s_debug_lock.unlock(InterruptState::Disabled);
			dprintln("Processor {} halted", Processor::current_id());
			if (InterruptController::is_initialized())
				InterruptController::get().broadcast_ipi();
			asm volatile("cli; 1: hlt; jmp 1b");
		}

		Thread::current().save_sse();

		if (InterruptController::get().is_in_service(irq))
		{
			InterruptController::get().eoi(irq);
			if (auto* handler = s_interruptables[irq])
				handler->handle_irq();
			else
				dprintln("no handler for irq 0x{2H}", irq);
		}

		auto& current_thread = Thread::current();
		if (current_thread.can_add_signal_to_execute())
			current_thread.handle_signal();

		Processor::scheduler().reschedule_if_idle();

		ASSERT(Thread::current().state() != Thread::State::Terminated);

		Thread::current().load_sse();
	}

	void IDT::register_interrupt_handler(uint8_t index, void (*handler)(), uint8_t ist)
	{
		auto& desc = m_idt[index];
		memset(&desc, 0, sizeof(GateDescriptor));

		desc.offset0 = (uint16_t)((uintptr_t)handler >> 0);
		desc.offset1 = (uint16_t)((uintptr_t)handler >> 16);
#if ARCH(x86_64)
		desc.offset2 = (uint32_t)((uintptr_t)handler >> 32);
		desc.IST = ist;
#else
		(void)ist;
#endif

		desc.selector = 0x08;
		desc.flags = 0x8E;
	}

	void IDT::register_syscall_handler(uint8_t index, void (*handler)())
	{
		register_interrupt_handler(index, handler);
		m_idt[index].flags = 0xEE;
	}

	void IDT::register_irq_handler(uint8_t irq, Interruptable* interruptable)
	{
		if (irq > s_interruptables.size())
			Kernel::panic("Trying to assign handler for irq {} while only {} are supported", irq, s_interruptables.size());
		s_interruptables[irq] = interruptable;
	}

#define X(num) extern "C" void isr ## num();
	ISR_LIST_X
#undef X

#define X(num) extern "C" void irq ## num();
	IRQ_LIST_X
#undef X

	extern "C" void asm_yield_handler();
	extern "C" void asm_ipi_handler();
	extern "C" void asm_timer_handler();
	extern "C" void asm_syscall_handler();

	IDT* IDT::create()
	{
		auto* idt = new IDT();
		ASSERT(idt);

		memset(idt->m_idt.data(), 0x00, 0x100 * sizeof(GateDescriptor));

#define X(num) idt->register_interrupt_handler(num, isr ## num);
		ISR_LIST_X
#undef X

#if ARCH(x86_64)
		idt->register_interrupt_handler(DoubleFault, isr8, 1);
		static_assert(DoubleFault == 8);
#endif

#define X(num) idt->register_interrupt_handler(IRQ_VECTOR_BASE + num, irq ## num);
		IRQ_LIST_X
#undef X

		idt->register_interrupt_handler(IRQ_YIELD, asm_yield_handler);
		idt->register_interrupt_handler(IRQ_IPI,   asm_ipi_handler);
		idt->register_interrupt_handler(IRQ_TIMER, asm_timer_handler);
		idt->register_syscall_handler(IRQ_SYSCALL, asm_syscall_handler);

		return idt;
	}

	[[noreturn]] void IDT::force_triple_fault()
	{
		// load 0 sized IDT and trigger an interrupt to force triple fault
		Processor::set_interrupt_state(InterruptState::Disabled);
		Processor::idt().m_idtr.size = 0;
		Processor::idt().load();
		asm volatile("int $0x00");
		ASSERT_NOT_REACHED();
	}

}
