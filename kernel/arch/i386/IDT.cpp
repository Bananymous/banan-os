#include <kernel/APIC.h>
#include <kernel/IDT.h>
#include <kernel/Panic.h>
#include <kernel/kprint.h>
#include <kernel/Serial.h>

union GateDescriptor
{
	struct
	{
		uint16_t	offset_lo;
		uint16_t	selector;
		uint8_t		reserved;
		uint8_t		type		: 4;
		uint8_t		zero		: 1;
		uint8_t		dpl			: 2;
		uint8_t		present		: 1;
		uint16_t	offset_hi;
	};

	struct
	{
		uint32_t low;
		uint32_t high;
	};
	
} __attribute__((packed));

struct IDTR
{
	uint16_t size;
	void* offset;
} __attribute((packed));

static IDTR				s_idtr;
static GateDescriptor	s_idt[0x100];

static void (*s_irq_handlers[0x100])() { nullptr };

#define INTERRUPT_HANDLER____(i, msg)													\
	static void interrupt ## i ()														\
	{																					\
		uint32_t eax, ebx, ecx, edx;													\
		uint32_t esp, ebp;																\
		uint32_t cr0, cr2, cr3, cr4;													\
		asm volatile("":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));						\
		asm volatile("movl %%esp, %%eax":"=a"(esp));									\
		asm volatile("movl %%ebp, %%eax":"=a"(ebp));									\
		asm volatile("movl %%cr0, %%eax":"=a"(cr0)); 									\
		asm volatile("movl %%cr2, %%eax":"=a"(cr2)); 									\
		asm volatile("movl %%cr3, %%eax":"=a"(cr3)); 									\
		asm volatile("movl %%cr4, %%eax":"=a"(cr4)); 									\
		Kernel::Panic(msg "\r\nRegister dump\r\n"										\
			"eax=0x{8H}, ebx=0x{8H}, ecx=0x{8H}, edx=0x{8H}\r\n" 						\
			"esp=0x{8H}, ebp=0x{8H}\r\n" 												\
			"CR0=0x{8H}, CR2=0x{8H}, CR3=0x{8H}, CR4=0x{8H}\r\n", 						\
			eax, ebx, ecx, edx, esp, ebp, cr0, cr2, cr3, cr4);							\
	}

#define INTERRUPT_HANDLER_ERR(i, msg)													\
	static void interrupt ## i ()														\
	{																					\
		uint32_t eax, ebx, ecx, edx;													\
		uint32_t esp, ebp;																\
		uint32_t cr0, cr2, cr3, cr4;													\
		uint32_t error_code;															\
		asm volatile("":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));						\
		asm volatile("movl %%esp, %%eax":"=a"(esp));									\
		asm volatile("movl %%ebp, %%eax":"=a"(ebp));									\
		asm volatile("movl %%cr0, %%eax":"=a"(cr0)); 									\
		asm volatile("movl %%cr2, %%eax":"=a"(cr2)); 									\
		asm volatile("movl %%cr3, %%eax":"=a"(cr3)); 									\
		asm volatile("movl %%cr4, %%eax":"=a"(cr4)); 									\
		asm volatile("popl %%eax":"=a"(error_code));									\
		Kernel::Panic(msg " (error code: 0x{8H})\r\n"									\
			"Register dump\r\n"															\
			"eax=0x{8H}, ebx=0x{8H}, ecx=0x{8H}, edx=0x{8H}\r\n" 						\
			"esp=0x{8H}, ebp=0x{8H}\r\n" 												\
			"CR0=0x{8H}, CR2=0x{8H}, CR3=0x{8H}, CR4=0x{8H}\r\n", 						\
			eax, ebx, ecx, edx, esp, ebp, cr0, cr2, cr3, cr4, error_code);				\
	}

INTERRUPT_HANDLER____(0x00, "Division Error")
INTERRUPT_HANDLER____(0x01, "Debug")
INTERRUPT_HANDLER____(0x02, "Non-maskable Interrupt")
INTERRUPT_HANDLER____(0x03, "Breakpoint")
INTERRUPT_HANDLER____(0x04, "Overflow")
INTERRUPT_HANDLER____(0x05, "Bound Range Exception")
INTERRUPT_HANDLER____(0x06, "Invalid Opcode")
INTERRUPT_HANDLER____(0x07, "Device Not Available")
INTERRUPT_HANDLER_ERR(0x08, "Double Fault")
INTERRUPT_HANDLER____(0x09, "Coprocessor Segment Overrun")
INTERRUPT_HANDLER_ERR(0x0A, "Invalid TSS")
INTERRUPT_HANDLER_ERR(0x0B, "Segment Not Present")
INTERRUPT_HANDLER_ERR(0x0C, "Stack-Segment Fault")
INTERRUPT_HANDLER_ERR(0x0D, "General Protection Fault")
INTERRUPT_HANDLER_ERR(0x0E, "Page Fault")
INTERRUPT_HANDLER____(0x0F, "Unknown Exception 0x0F")
INTERRUPT_HANDLER____(0x10, "x87 Floating-Point Exception")
INTERRUPT_HANDLER_ERR(0x11, "Alignment Check")
INTERRUPT_HANDLER____(0x12, "Machine Check")
INTERRUPT_HANDLER____(0x13, "SIMD Floating-Point Exception")
INTERRUPT_HANDLER____(0x14, "Virtualization Exception")
INTERRUPT_HANDLER_ERR(0x15, "Control Protection Exception")
INTERRUPT_HANDLER____(0x16, "Unknown Exception 0x16")
INTERRUPT_HANDLER____(0x17, "Unknown Exception 0x17")
INTERRUPT_HANDLER____(0x18, "Unknown Exception 0x18")
INTERRUPT_HANDLER____(0x19, "Unknown Exception 0x19")
INTERRUPT_HANDLER____(0x1A, "Unknown Exception 0x1A")
INTERRUPT_HANDLER____(0x1B, "Unknown Exception 0x1B")
INTERRUPT_HANDLER____(0x1C, "Hypervisor Injection Exception")
INTERRUPT_HANDLER_ERR(0x1D, "VMM Communication Exception")
INTERRUPT_HANDLER_ERR(0x1E, "Security Exception")
INTERRUPT_HANDLER____(0x1F, "Unkown Exception 0x1F")

#define REGISTER_HANDLER(i) register_interrupt_handler(i, interrupt ## i)

extern "C" void handle_irq()
{
	uint32_t isr[8];
	APIC::GetISR(isr);

	uint8_t irq = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		for (uint8_t j = 0; j < 32; j++)
		{
			if (isr[i] & ((uint32_t)1 << j))
			{
				irq = 32 * i + j;
				goto found;
			}
		}
	}

found:
	if (irq == 0)
	{
		dprintln("Spurious irq");
		return;	
	}

    if (s_irq_handlers[irq])
        s_irq_handlers[irq]();
	else
		Kernel::Panic("no handler for irq 0x{2H}\n", irq);

	APIC::EOI(irq);
}

extern "C" void handle_irq_common();
asm(
".globl handle_irq_common;"
"handle_irq_common:"
	"pusha;"
	"pushw %ds;"
	"pushw %es;"
	"pushw %ss;"
	"pushw %ss;"
	"popw %ds;"
	"popw %es;"
	"call handle_irq;"
	"popw %es;"
	"popw %ds;"
	"popa;"
	"iret;"
);


namespace IDT
{

	static void flush_idt()
	{
		asm volatile("lidt %0"::"m"(s_idtr));
	}

	static void register_interrupt_handler(uint8_t index, void (*f)())
	{
		s_idt[index].low = 0x00080000 | ((uint32_t)(f) & 0x0000ffff);
		s_idt[index].high = ((uint32_t)(f) & 0xffff0000) | 0x8e00;
	}

	void register_irq_handler(uint8_t irq, void (*f)())
	{
		s_irq_handlers[IRQ_VECTOR_BASE + irq] = f;
		register_interrupt_handler(IRQ_VECTOR_BASE + irq, handle_irq_common);
		flush_idt();
	}

	void initialize()
	{
		s_idtr.offset = s_idt;
		s_idtr.size = sizeof(s_idt) - 1;

		for (uint8_t i = 0xFF; i > IRQ_VECTOR_BASE; i--)
			register_irq_handler(i, nullptr);

		REGISTER_HANDLER(0x00);
		REGISTER_HANDLER(0x01);
		REGISTER_HANDLER(0x02);
		REGISTER_HANDLER(0x03);
		REGISTER_HANDLER(0x04);
		REGISTER_HANDLER(0x05);
		REGISTER_HANDLER(0x06);
		REGISTER_HANDLER(0x07);
		REGISTER_HANDLER(0x08);
		REGISTER_HANDLER(0x09);
		REGISTER_HANDLER(0x0A);
		REGISTER_HANDLER(0x0B);
		REGISTER_HANDLER(0x0C);
		REGISTER_HANDLER(0x0D);
		REGISTER_HANDLER(0x0E);
		REGISTER_HANDLER(0x0F);
		REGISTER_HANDLER(0x10);
		REGISTER_HANDLER(0x11);
		REGISTER_HANDLER(0x12);
		REGISTER_HANDLER(0x13);
		REGISTER_HANDLER(0x14);
		REGISTER_HANDLER(0x15);
		REGISTER_HANDLER(0x16);
		REGISTER_HANDLER(0x17);
		REGISTER_HANDLER(0x18);
		REGISTER_HANDLER(0x19);
		REGISTER_HANDLER(0x1A);
		REGISTER_HANDLER(0x1B);
		REGISTER_HANDLER(0x1C);
		REGISTER_HANDLER(0x1D);
		REGISTER_HANDLER(0x1E);
		REGISTER_HANDLER(0x1F);

		flush_idt();
	}

}