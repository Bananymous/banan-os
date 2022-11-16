#include <kernel/GDT.h>
#include <kernel/kmalloc.h>

struct GDTR
{
	uint16_t size;
	void* address;
} __attribute__((packed));

static GDTR					s_gdtr;
static SegmentDesriptor*	s_gdt;

void write_gdt_entry_raw(uint8_t index, uint32_t low, uint32_t high)
{
	s_gdt[index].low = low;
	s_gdt[index].high = high;
}

void write_gdt_entry(uint8_t index, SegmentDesriptor descriptor)
{
	write_gdt_entry_raw(index, descriptor.low, descriptor.high);
}

static void flush_gdt()
{
	asm volatile("lgdt %0"::"m"(s_gdtr));
}

void gdt_initialize()
{
	constexpr size_t gdt_size = 256;

	s_gdt = new SegmentDesriptor[gdt_size];
	
	s_gdtr.address = s_gdt;
	s_gdtr.size = gdt_size * 8 - 1;

	uint8_t index = 0;
	write_gdt_entry(index++, { 0, 0x0000, 0x00, 0x0 }); // null
	write_gdt_entry(index++, { 0, 0xFFFF, 0x9A, 0xC }); // kernel code
	write_gdt_entry(index++, { 0, 0xFFFF, 0x92, 0xC }); // kernel data
	write_gdt_entry(index++, { 0, 0xFFFF, 0xFA, 0xC }); // user code
	write_gdt_entry(index++, { 0, 0xFFFF, 0xF2, 0xC }); // user data
	
	flush_gdt();
}
