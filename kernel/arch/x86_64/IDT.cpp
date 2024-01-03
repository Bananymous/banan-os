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
#define IRQ_LIST_X X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31)

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
				auto& istack = Thread::current().interrupt_stack();
				if (stack.vaddr() < interrupt_stack.rsp && interrupt_stack.rsp <= stack.vaddr() + stack.size())
					; // using normal stack
				else if (istack.vaddr() < interrupt_stack.rsp && interrupt_stack.rsp <= istack.vaddr() + istack.size())
					; // using interrupt stack
				else
				{
					derrorln("Stack pointer out of bounds!");
					derrorln("rsp {H}, stack {H}->{H}, istack {H}->{H}",
						interrupt_stack.rsp,
						stack.vaddr(), stack.vaddr() + stack.size(),
						istack.vaddr(), istack.vaddr() + istack.size()
					);
					Thread::current().handle_signal(SIGKILL);
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
						Thread::current().handle_signal(SIGKILL);
						goto done;
					}
				}
			}
#if __enable_sse
			else if (isr == ISR::DeviceNotAvailable)
			{
				asm volatile(
					"movq %cr0, %rax;"
					"andq $~(1 << 3), %rax;"
					"movq %rax, %cr0;"
				);
				if (auto* current = &Thread::current(); current != Thread::sse_thread())
				{
					if (auto* sse = Thread::sse_thread())
						sse->save_sse();
					current->load_sse();
				}
				goto done;
			}
#endif
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
		return;
	}

	extern "C" void cpp_irq_handler(uint64_t irq, InterruptStack& interrupt_stack)
	{
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

	extern "C" void syscall_asm();

	void initialize()
	{
		s_idt = (GateDescriptor*)kmalloc(0x100 * sizeof(GateDescriptor));
		ASSERT(s_idt);
		memset(s_idt, 0x00, 0x100 * sizeof(GateDescriptor));

		s_idtr.offset = (uint64_t)s_idt;
		s_idtr.size = 0x100 * sizeof(GateDescriptor) - 1;

#define X(num) register_interrupt_handler(num, isr ## num);
		ISR_LIST_X
#undef X

#define X(num) register_interrupt_handler(IRQ_VECTOR_BASE + num, irq ## num);
		IRQ_LIST_X
#undef X

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
