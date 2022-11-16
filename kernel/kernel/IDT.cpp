#include <kernel/IDT.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/kprint.h>

struct IDTR
{
	uint16_t size;
	void* offset;
} __attribute((packed));

static IDTR				s_idtr;
static GateDescriptor*	s_idt;


#define INTERRUPT_HANDLER(i, msg)													\
	static void interrupt ## i ()													\
	{																				\
		uint32_t cr0, cr2, cr3, cr4;												\
		asm volatile("movl %%cr0, %%eax":"=a"(cr0)); 								\
		asm volatile("movl %%cr2, %%eax":"=a"(cr2)); 								\
		asm volatile("movl %%cr3, %%eax":"=a"(cr3)); 								\
		asm volatile("movl %%cr4, %%eax":"=a"(cr4)); 								\
		Kernel::panic(msg ", CR0={} CR2={} CR3={} CR4={}\n", cr0, cr2, cr3, cr4);	\
	}

INTERRUPT_HANDLER(0x00, "Divide error")
INTERRUPT_HANDLER(0x01, "Debug exception")
INTERRUPT_HANDLER(0x02, "Unknown error")
INTERRUPT_HANDLER(0x03, "Breakpoint")
INTERRUPT_HANDLER(0x04, "Overflow")
INTERRUPT_HANDLER(0x05, "Bounds check")
INTERRUPT_HANDLER(0x06, "Invalid opcode")
INTERRUPT_HANDLER(0x07, "Coprocessor not available")
INTERRUPT_HANDLER(0x08, "Double fault")
INTERRUPT_HANDLER(0x09, "Coprocessor segment overrun")
INTERRUPT_HANDLER(0x0a, "Invalid TSS")
INTERRUPT_HANDLER(0x0b, "Segment not present")
INTERRUPT_HANDLER(0x0c, "Stack exception")
INTERRUPT_HANDLER(0x0d, "General protection fault")
INTERRUPT_HANDLER(0x0e, "Page fault")
INTERRUPT_HANDLER(0x0f, "Unknown error")
INTERRUPT_HANDLER(0x10, "Coprocessor error")

#define REGISTER_HANDLER(i) register_interrupt_handeler(i, interrupt ## i)

static void flush_idt()
{
	asm volatile("lidt %0"::"m"(s_idtr));
}

static void unimplemented_trap()
{
	Kernel::panic("Unhandeled IRQ");
}

static void register_interrupt_handeler(uint8_t index, void (*f)())
{
	s_idt[index].low = 0x00080000 | ((uint32_t)(f) & 0x0000ffff);
	s_idt[index].high = ((uint32_t)(f) & 0xffff0000) | 0x8e00;
	flush_idt();
}

void idt_initialize()
{
	constexpr size_t idt_size = 256;

	s_idt = new GateDescriptor[idt_size];

	s_idtr.offset = s_idt;
	s_idtr.size = idt_size * 8;

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
	REGISTER_HANDLER(0x0a);
	REGISTER_HANDLER(0x0b);
	REGISTER_HANDLER(0x0c);
	REGISTER_HANDLER(0x0d);
	REGISTER_HANDLER(0x0e);
	REGISTER_HANDLER(0x0f);
	REGISTER_HANDLER(0x10);

	for (uint16_t i = 0x11; i < idt_size; i++)
		register_interrupt_handeler(i, unimplemented_trap);

	flush_idt();
}
