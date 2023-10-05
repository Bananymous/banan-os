#include <BAN/Errors.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Panic.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/PIT.h>

#include <unistd.h>

#define REGISTER_ISR_HANDLER(i) register_interrupt_handler(i, isr ## i)
#define REGISTER_IRQ_HANDLER(i) register_interrupt_handler(IRQ_VECTOR_BASE + i, irq ## i)

namespace Kernel::IDT
{

	struct Registers
	{
		uint64_t rsp;
		uint64_t rip;
		uint64_t rflags;
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
		uint64_t rsi;
		uint64_t rdi;
		uint64_t rbp;
		uint64_t rdx;
		uint64_t rcx;
		uint64_t rbx;
		uint64_t rax;
	};

	struct GateDescriptor
	{
		uint16_t offset1;
		uint16_t selector;
		uint8_t IST;
		uint8_t flags;
		uint16_t offset2;
		uint32_t offset3;
		uint32_t reserved;
	} __attribute__((packed));

	struct IDTR
	{
		uint16_t size;
		uint64_t offset;
	} __attribute__((packed));

	static IDTR				s_idtr;
	static GateDescriptor*	s_idt = nullptr;

	static Interruptable* s_interruptables[0x10] {};

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

	extern "C" void cpp_isr_handler(uint64_t isr, uint64_t error, InterruptStack& interrupt_stack, const Registers* regs)
	{
#if __enable_sse
		bool from_userspace = (interrupt_stack.cs & 0b11) == 0b11;
		if (from_userspace)
			Thread::current().save_sse();
#endif

		pid_t tid = Scheduler::current_tid();
		pid_t pid = tid ? Process::current().pid() : 0;

		if (tid)
		{
			Thread::current().set_return_rsp(interrupt_stack.rsp);
			Thread::current().set_return_rip(interrupt_stack.rip);

			if (isr == ISR::PageFault)
			{
				// Check if stack is OOB
				auto& stack = Thread::current().stack();
				if (interrupt_stack.rsp < stack.vaddr())
				{
					derrorln("Stack overflow");
					goto done;
				}
				if (interrupt_stack.rsp >= stack.vaddr() + stack.size())
				{
					derrorln("Stack underflow");
					goto done;
				}

				// Try demand paging on non present pages
				PageFaultError page_fault_error;
				page_fault_error.raw = error;
				if (!page_fault_error.present)
				{
					asm volatile("sti");
					auto result = Process::current().allocate_page_for_demand_paging(regs->cr2);
					asm volatile("cli");

					if (!result.is_error() && result.value())
						goto done;

					if (result.is_error())
					{
						dwarnln("Demand paging: {}", result.error());
						Thread::current().handle_signal(SIGTERM);
						goto done;
					}
				}
			}
		}

		if (PageTable::current().get_page_flags(interrupt_stack.rip & PAGE_ADDR_MASK) & PageTable::Flags::Present)
		{
			auto* machine_code = (const uint8_t*)interrupt_stack.rip;
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

		dwarnln(
			"{} (error code: 0x{16H}), pid {}, tid {}\r\n"
			"Register dump\r\n"
			"rax=0x{16H}, rbx=0x{16H}, rcx=0x{16H}, rdx=0x{16H}\r\n"
			"rsp=0x{16H}, rbp=0x{16H}, rdi=0x{16H}, rsi=0x{16H}\r\n"
			"rip=0x{16H}, rflags=0x{16H}\r\n"
			"cr0=0x{16H}, cr2=0x{16H}, cr3=0x{16H}, cr4=0x{16H}",
			isr_exceptions[isr], error, pid, tid,
			regs->rax, regs->rbx, regs->rcx, regs->rdx,
			regs->rsp, regs->rbp, regs->rdi, regs->rsi,
			regs->rip, regs->rflags,
			regs->cr0, regs->cr2, regs->cr3, regs->cr4
		);
		if (isr == ISR::PageFault)
			PageTable::current().debug_dump();
		Debug::dump_stack_trace();

		if (tid && Thread::current().is_userspace())
		{
			// TODO: Confirm and fix the exception to signal mappings

			int signal = 0;
			switch (isr)
			{
			case ISR::DeviceNotAvailable:
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
#if __enable_sse
		if (from_userspace)
		{
			ASSERT(Thread::current().state() == Thread::State::Executing);
			Thread::current().load_sse();
		}
#endif
		return;
	}

	extern "C" void cpp_irq_handler(uint64_t irq, InterruptStack& interrupt_stack)
	{
#if __enable_sse
		bool from_userspace = (interrupt_stack.cs & 0b11) == 0b11;
		if (from_userspace)
			Thread::current().save_sse();
#endif

		if (Scheduler::current_tid())
		{
			Thread::current().set_return_rsp(interrupt_stack.rsp);
			Thread::current().set_return_rip(interrupt_stack.rip);
		}

		if (!InterruptController::get().is_in_service(irq))
			dprintln("spurious irq 0x{2H}", irq);
		else
		{
			InterruptController::get().eoi(irq);
			if (s_interruptables[irq])
				s_interruptables[irq]->handle_irq();
			else
				dprintln("no handler for irq 0x{2H}\n", irq);
		}

		Scheduler::get().reschedule_if_idling();

		ASSERT(Thread::current().state() != Thread::State::Terminated);

#if __enable_sse
		if (from_userspace)
		{
			ASSERT(Thread::current().state() == Thread::State::Executing);
			Thread::current().load_sse();
		}
#endif
	}

	static void flush_idt()
	{
		asm volatile("lidt %0"::"m"(s_idtr));
	}

	static void register_interrupt_handler(uint8_t index, void(*handler)())
	{
		GateDescriptor& descriptor = s_idt[index];
		descriptor.offset1 = (uint16_t)((uint64_t)handler >> 0);
		descriptor.offset2 = (uint16_t)((uint64_t)handler >> 16);
		descriptor.offset3 = (uint32_t)((uint64_t)handler >> 32);

		descriptor.selector = 0x08;
		descriptor.IST = 0;
		descriptor.flags = 0x8E;
	}

	static void register_syscall_handler(uint8_t index, void(*handler)())
	{
		register_interrupt_handler(index, handler);
		s_idt[index].flags = 0xEE;
	}

	void register_irq_handler(uint8_t irq, Interruptable* interruptable)
	{
		s_interruptables[irq] = interruptable;
	}

	extern "C" void isr0();
	extern "C" void isr1();
	extern "C" void isr2();
	extern "C" void isr3();
	extern "C" void isr4();
	extern "C" void isr5();
	extern "C" void isr6();
	extern "C" void isr7();
	extern "C" void isr8();
	extern "C" void isr9();
	extern "C" void isr10();
	extern "C" void isr11();
	extern "C" void isr12();
	extern "C" void isr13();
	extern "C" void isr14();
	extern "C" void isr15();
	extern "C" void isr16();
	extern "C" void isr17();
	extern "C" void isr18();
	extern "C" void isr19();
	extern "C" void isr20();
	extern "C" void isr21();
	extern "C" void isr22();
	extern "C" void isr23();
	extern "C" void isr24();
	extern "C" void isr25();
	extern "C" void isr26();
	extern "C" void isr27();
	extern "C" void isr28();
	extern "C" void isr29();
	extern "C" void isr30();
	extern "C" void isr31();

	extern "C" void irq0();
	extern "C" void irq1();
	extern "C" void irq2();
	extern "C" void irq3();
	extern "C" void irq4();
	extern "C" void irq5();
	extern "C" void irq6();
	extern "C" void irq7();
	extern "C" void irq8();
	extern "C" void irq9();
	extern "C" void irq10();
	extern "C" void irq11();
	extern "C" void irq12();
	extern "C" void irq13();
	extern "C" void irq14();
	extern "C" void irq15();

	extern "C" void syscall_asm();

	void initialize()
	{
		s_idt = (GateDescriptor*)kmalloc(0x100 * sizeof(GateDescriptor));
		ASSERT(s_idt);
		memset(s_idt, 0x00, 0x100 * sizeof(GateDescriptor));

		s_idtr.offset = (uint64_t)s_idt;
		s_idtr.size = 0x100 * sizeof(GateDescriptor) - 1;

		REGISTER_ISR_HANDLER(0);
		REGISTER_ISR_HANDLER(1);
		REGISTER_ISR_HANDLER(2);
		REGISTER_ISR_HANDLER(3);
		REGISTER_ISR_HANDLER(4);
		REGISTER_ISR_HANDLER(5);
		REGISTER_ISR_HANDLER(6);
		REGISTER_ISR_HANDLER(7);
		REGISTER_ISR_HANDLER(8);
		REGISTER_ISR_HANDLER(9);
		REGISTER_ISR_HANDLER(10);
		REGISTER_ISR_HANDLER(11);
		REGISTER_ISR_HANDLER(12);
		REGISTER_ISR_HANDLER(13);
		REGISTER_ISR_HANDLER(14);
		REGISTER_ISR_HANDLER(15);
		REGISTER_ISR_HANDLER(16);
		REGISTER_ISR_HANDLER(17);
		REGISTER_ISR_HANDLER(18);
		REGISTER_ISR_HANDLER(19);
		REGISTER_ISR_HANDLER(20);
		REGISTER_ISR_HANDLER(21);
		REGISTER_ISR_HANDLER(22);
		REGISTER_ISR_HANDLER(23);
		REGISTER_ISR_HANDLER(24);
		REGISTER_ISR_HANDLER(25);
		REGISTER_ISR_HANDLER(26);
		REGISTER_ISR_HANDLER(27);
		REGISTER_ISR_HANDLER(28);
		REGISTER_ISR_HANDLER(29);
		REGISTER_ISR_HANDLER(30);
		REGISTER_ISR_HANDLER(31);

		REGISTER_IRQ_HANDLER(0);
		REGISTER_IRQ_HANDLER(1);
		REGISTER_IRQ_HANDLER(2);
		REGISTER_IRQ_HANDLER(3);
		REGISTER_IRQ_HANDLER(4);
		REGISTER_IRQ_HANDLER(5);
		REGISTER_IRQ_HANDLER(6);
		REGISTER_IRQ_HANDLER(7);
		REGISTER_IRQ_HANDLER(8);
		REGISTER_IRQ_HANDLER(9);
		REGISTER_IRQ_HANDLER(10);
		REGISTER_IRQ_HANDLER(11);
		REGISTER_IRQ_HANDLER(12);
		REGISTER_IRQ_HANDLER(13);
		REGISTER_IRQ_HANDLER(14);
		REGISTER_IRQ_HANDLER(15);

		register_syscall_handler(0x80, syscall_asm);

		flush_idt();
	}

	[[noreturn]] void force_triple_fault()
	{
		// load 0 sized IDT and trigger an interrupt to force triple fault
		asm volatile("cli");
		s_idtr.size = 0;
		flush_idt();
		asm volatile("int $0x00");
		ASSERT_NOT_REACHED();
	}

}
