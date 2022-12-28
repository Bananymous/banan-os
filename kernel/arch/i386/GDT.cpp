#include <kernel/GDT.h>
#include <kernel/kmalloc.h>

struct GDTR
{
	uint16_t size;
	void* address;
} __attribute__((packed));

static GDTR					s_gdtr;
static SegmentDesriptor		s_gdt[5];

extern "C" void load_gdt(void* gdt_ptr);
asm(
".global load_gdt;"
"load_gdt:"
	"movl 4(%esp),%eax;"
	"lgdt (%eax);"

	"movw $0x10, %ax;"
	"movw %ax, %ds;"
	"movw %ax, %es;"
	"movw %ax, %fs;"
	"movw %ax, %gs;"
	"movw %ax, %ss;"
	"jmp  $0x08,$flush;"

"flush:"
	"ret;"
);

void write_gdt_entry_raw(uint8_t segment, uint32_t low, uint32_t high)
{
	uint8_t index = segment >> 3;
	s_gdt[index].low = low;
	s_gdt[index].high = high;
}

void write_gdt_entry(uint8_t segment, SegmentDesriptor descriptor)
{
	write_gdt_entry_raw(segment, descriptor.low, descriptor.high);
}

void gdt_initialize()
{
	s_gdtr.address = s_gdt;
	s_gdtr.size = sizeof(s_gdt) - 1;

	write_gdt_entry(0x00, { 0, 0x00000, 0x00, 0x0 }); // null
	write_gdt_entry(0x08, { 0, 0xFFFFF, 0x9A, 0xC }); // kernel code
	write_gdt_entry(0x10, { 0, 0xFFFFF, 0x92, 0xC }); // kernel data
	write_gdt_entry(0x18, { 0, 0xFFFFF, 0xFA, 0xC }); // user code
	write_gdt_entry(0x20, { 0, 0xFFFFF, 0xF2, 0xC }); // user data
	
	load_gdt(&s_gdtr);
}
