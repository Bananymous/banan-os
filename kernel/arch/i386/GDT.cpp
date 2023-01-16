#include <kernel/GDT.h>

#include <stdint.h>

namespace GDT
{

	union SegmentDesriptor
	{
		struct
		{
			uint16_t	limit_lo;
			uint16_t	base_lo;
			uint8_t		base_hi1;

			uint8_t		type			: 4;
			uint8_t		system			: 1;
			uint8_t		DPL				: 2;
			uint8_t		present			: 1;

			uint8_t		limit_hi		: 4;
			uint8_t		flags			: 4;

			uint8_t		base_hi2;
		} __attribute__((packed));
		
		struct
		{
			uint32_t low;
			uint32_t high;
		} __attribute__((packed));

		SegmentDesriptor() : low(0), high(0) {}
		SegmentDesriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t _flags)
			: low(0), high(0)
		{
			set_base(base);
			set_limit(limit);

			high |= ((uint16_t)access) << 8;
			flags = _flags;
		}

		void set_base(uint32_t base)
		{
			base_lo  =  base        & 0xFFFF;
			base_hi1 = (base >> 16) & 0x00FF;
			base_hi2 = (base >> 24) & 0x00FF;
		}

		void set_limit(uint32_t limit)
		{
			limit_lo =  limit        & 0xFFFF;
			limit_hi = (limit >> 16) & 0x00FF;
		}

	} __attribute__((packed));

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

	void write_entry_raw(uint8_t segment, uint32_t low, uint32_t high)
	{
		uint8_t index = segment >> 3;
		s_gdt[index].low = low;
		s_gdt[index].high = high;
	}

	void write_entry(uint8_t segment, SegmentDesriptor descriptor)
	{
		write_entry_raw(segment, descriptor.low, descriptor.high);
	}

	void initialize()
	{
		s_gdtr.address = s_gdt;
		s_gdtr.size = sizeof(s_gdt) - 1;

		write_entry(0x00, { 0, 0x00000, 0x00, 0x0 }); // null
		write_entry(0x08, { 0, 0xFFFFF, 0x9A, 0xC }); // kernel code
		write_entry(0x10, { 0, 0xFFFFF, 0x92, 0xC }); // kernel data
		write_entry(0x18, { 0, 0xFFFFF, 0xFA, 0xC }); // user code
		write_entry(0x20, { 0, 0xFFFFF, 0xF2, 0xC }); // user data
		
		load_gdt(&s_gdtr);
	}

}