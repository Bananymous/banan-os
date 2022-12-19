#include <kernel/APIC.h>
#include <kernel/IDT.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
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

static void (*s_irq_handlers[0xFF])() { nullptr };

#define INTERRUPT_HANDLER(i, msg)												\
	static void interrupt ## i ()												\
	{																			\
		uint32_t cr0, cr2, cr3, cr4;											\
		asm volatile("movl %%cr0, %%eax":"=a"(cr0)); 							\
		asm volatile("movl %%cr2, %%eax":"=a"(cr2)); 							\
		asm volatile("movl %%cr3, %%eax":"=a"(cr3)); 							\
		asm volatile("movl %%cr4, %%eax":"=a"(cr4)); 							\
		Kernel::panic(msg ", CR0={} CR2={} CR3={} CR4={}", cr0, cr2, cr3, cr4);	\
	}

INTERRUPT_HANDLER(0x00, "Division Error")
INTERRUPT_HANDLER(0x01, "Debug")
INTERRUPT_HANDLER(0x02, "Non-maskable Interrupt")
INTERRUPT_HANDLER(0x03, "Breakpoint")
INTERRUPT_HANDLER(0x04, "Overflow")
INTERRUPT_HANDLER(0x05, "Bound Range Exception")
INTERRUPT_HANDLER(0x06, "Invalid Opcode")
INTERRUPT_HANDLER(0x07, "Device Not Available")
INTERRUPT_HANDLER(0x08, "Double Fault")
INTERRUPT_HANDLER(0x09, "Coprocessor Segment Overrun")
INTERRUPT_HANDLER(0x0A, "Invalid TSS")
INTERRUPT_HANDLER(0x0B, "Segment Not Present")
INTERRUPT_HANDLER(0x0C, "Stack-Segment Fault")
INTERRUPT_HANDLER(0x0D, "Stack-Segment Fault")
INTERRUPT_HANDLER(0x0E, "Page Fault")
INTERRUPT_HANDLER(0x0F, "Unknown Exception 0x0F")
INTERRUPT_HANDLER(0x10, "x87 Floating-Point Exception")
INTERRUPT_HANDLER(0x11, "Alignment Check")
INTERRUPT_HANDLER(0x12, "Machine Check")
INTERRUPT_HANDLER(0x13, "SIMD Floating-Point Exception")
INTERRUPT_HANDLER(0x14, "Virtualization Exception")
INTERRUPT_HANDLER(0x15, "Control Protection Exception")
INTERRUPT_HANDLER(0x16, "Unknown Exception 0x16")
INTERRUPT_HANDLER(0x17, "Unknown Exception 0x17")
INTERRUPT_HANDLER(0x18, "Unknown Exception 0x18")
INTERRUPT_HANDLER(0x19, "Unknown Exception 0x19")
INTERRUPT_HANDLER(0x1A, "Unknown Exception 0x1A")
INTERRUPT_HANDLER(0x1B, "Unknown Exception 0x1B")
INTERRUPT_HANDLER(0x1C, "Hypervisor Injection Exception")
INTERRUPT_HANDLER(0x1D, "VMM Communication Exception")
INTERRUPT_HANDLER(0x1E, "Security Exception")
INTERRUPT_HANDLER(0x1F, "Unkown Exception 0x1F")


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
		Kernel::panic("no handler for irq 0x{2H}\n", irq);

	APIC::EOI();
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

	static void unimplemented_trap()
	{
		Kernel::panic("Unhandeled IRQ");
	}

	static void register_interrupt_handler(uint8_t index, void (*f)())
	{
		s_idt[index].low = 0x00080000 | ((uint32_t)(f) & 0x0000ffff);
		s_idt[index].high = ((uint32_t)(f) & 0xffff0000) | 0x8e00;
		flush_idt();
	}

	void register_irq_handler(uint8_t irq, void (*f)())
	{
		s_irq_handlers[IRQ_VECTOR_BASE + irq] = f;
		register_interrupt_handler(IRQ_VECTOR_BASE + irq, handle_irq_common);
	}

	void initialize()
	{
		s_idtr.offset = s_idt;
		s_idtr.size = sizeof(s_idt);

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