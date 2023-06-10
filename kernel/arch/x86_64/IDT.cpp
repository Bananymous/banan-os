#include <BAN/Errors.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Panic.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>

#include <unistd.h>

#define REGISTER_ISR_HANDLER(i) register_interrupt_handler(i, isr ## i)
#define REGISTER_IRQ_HANDLER(i) register_interrupt_handler(IRQ_VECTOR_BASE + i, irq ## i)

namespace IDT
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

	static void(*s_irq_handlers[0x10])() { nullptr };

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

	extern "C" void cpp_isr_handler(uint64_t isr, uint64_t error, const Registers* regs)
	{
		pid_t tid = Kernel::Scheduler::current_tid();
		pid_t pid = tid ? Kernel::Process::current().pid() : 0;

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

		if (tid && Kernel::Thread::current().is_userspace() && !Kernel::Thread::current().is_in_syscall())
		{
			auto message = BAN::String::formatted("{}, aborting\n", isr_exceptions[isr]);
			(void)Kernel::Process::current().write(STDERR_FILENO, message.data(), message.size());
			asm volatile("sti");
			Kernel::Process::current().exit(1);
		}
		else
		{
			Kernel::panic("Unhandled exception");
		}
	}

	extern "C" void cpp_irq_handler(uint64_t irq)
	{
		if (s_irq_handlers[irq])
			s_irq_handlers[irq]();
		else
		{
			if (!InterruptController::get().is_in_service(irq))
			{
				dprintln("spurious irq 0x{2H}", irq);
				return;
			}
			dprintln("no handler for irq 0x{2H}\n", irq);
		}

		// NOTE: Scheduler sends PIT eoi's
		if (irq != PIT_IRQ)
			InterruptController::get().eoi(irq);

		Kernel::Scheduler::get().reschedule_if_idling();
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

	void register_irq_handler(uint8_t irq, void(*handler)())
	{
		s_irq_handlers[irq] = handler;
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

}