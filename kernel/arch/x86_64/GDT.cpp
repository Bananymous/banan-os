#include <BAN/Array.h>
#include <kernel/GDT.h>

#include <string.h>

namespace Kernel::GDT
{

	struct TaskStateSegment
	{
		uint32_t reserved1;
		uint64_t rsp0;
		uint64_t rsp1;
		uint64_t rsp2;
		uint64_t reserved2;
		uint64_t ist1;
		uint64_t ist2;
		uint64_t ist3;
		uint64_t ist4;
		uint64_t ist5;
		uint64_t ist6;
		uint64_t ist7;
		uint64_t reserved3;
		uint16_t reserved4;
		uint16_t iopb;
	} __attribute__((packed));

	union SegmentDescriptor
	{
		struct
		{
			uint16_t	limit1;
			uint16_t	base1;
			uint8_t		base2;
			uint8_t		access;
			uint8_t		limit2 : 4;
			uint8_t		flags  : 4;
			uint8_t		base3;
		} __attribute__((packed));

		struct
		{
			uint32_t low;
			uint32_t high;
		} __attribute__((packed));

	} __attribute__((packed));

	struct GDTR
	{
		uint16_t size;
		uint64_t address;
	} __attribute__((packed));

	static constexpr uint16_t s_tss_offset = 0x28;

	static TaskStateSegment s_tss;
	static BAN::Array<SegmentDescriptor, 7> s_gdt; // null, kernel code, kernel data, user code, user data, tss low, tss high
	static GDTR s_gdtr;

	static void write_entry(uint8_t offset, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
	{
		ASSERT(offset % sizeof(SegmentDescriptor) == 0);

		SegmentDescriptor& desc = s_gdt[offset / sizeof(SegmentDescriptor)];
		desc.base1 = (base >>  0) & 0xFFFF;
		desc.base2 = (base >> 16) & 0xFF;
		desc.base3 = (base >> 24) & 0xFF;

		desc.limit1 = (limit >>  0) & 0xFFFF;
		desc.limit2 = (limit >> 16) & 0x0F;

		desc.access = access & 0xFF;

		desc.flags = flags & 0x0F;
	}

	static void write_tss()
	{
		memset(&s_tss, 0x00, sizeof(TaskStateSegment));
		s_tss.iopb = sizeof(TaskStateSegment);

		uint64_t base = (uint64_t)&s_tss;

		write_entry(s_tss_offset, (uint32_t)base, sizeof(TaskStateSegment), 0x89, 0x0);

		SegmentDescriptor& desc = s_gdt[s_tss_offset / sizeof(SegmentDescriptor) + 1];
		desc.low = base >> 32;
		desc.high = 0;
	}

	void set_tss_stack(uintptr_t rsp)
	{
		s_tss.rsp0 = rsp;
	}

	static void flush_gdt()
	{
		asm volatile("lgdt %0" :: "m"(s_gdtr));
	}

	static void flush_tss()
	{
		asm volatile("ltr %0" :: "m"(s_tss_offset));
	}

	void initialize()
	{
		s_gdtr.address = (uint64_t)&s_gdt;
		s_gdtr.size = s_gdt.size() * sizeof(SegmentDescriptor) - 1;

		write_entry(0x00, 0x00000000, 0x00000, 0x00, 0x0); // null
		write_entry(0x08, 0x00000000, 0xFFFFF, 0x9A, 0xA); // kernel code
		write_entry(0x10, 0x00000000, 0xFFFFF, 0x92, 0xC); // kernel data
		write_entry(0x18, 0x00000000, 0xFFFFF, 0xFA, 0xA); // user code
		write_entry(0x20, 0x00000000, 0xFFFFF, 0xF2, 0xC); // user data
		write_tss();

		flush_gdt();
		flush_tss();
	}

}
