#include <kernel/GDT.h>
#include <kernel/kmalloc.h>

struct GDTR
{
	uint16_t size;
	void* address;
} __attribute__((packed));

static GDTR					s_gdtr;
static SegmentDesriptor*	s_gdt;

extern "C" void load_gdt(void* gdt_ptr);

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
	constexpr uint8_t GDT_SIZE = 5;

	s_gdt = new SegmentDesriptor[GDT_SIZE];

	s_gdtr.address = s_gdt;
	s_gdtr.size = GDT_SIZE * 8 - 1;

	write_gdt_entry(0x00, { 0, 0x00000, 0x00, 0x0 }); // null
	write_gdt_entry(0x08, { 0, 0xFFFFF, 0x9A, 0xC }); // kernel code
	write_gdt_entry(0x10, { 0, 0xFFFFF, 0x92, 0xC }); // kernel data
	write_gdt_entry(0x18, { 0, 0xFFFFF, 0xFA, 0xC }); // user code
	write_gdt_entry(0x20, { 0, 0xFFFFF, 0xF2, 0xC }); // user data
	
	load_gdt(&s_gdtr);
}
