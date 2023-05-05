#include <BAN/Assert.h>
#include <kernel/GDT.h>

#include <string.h>

extern "C" uintptr_t g_boot_stack_top[0];

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

	static TaskStateSegment* s_tss = nullptr;
	static SegmentDescriptor* s_gdt = nullptr;
	static GDTR s_gdtr;

	static void write_entry(uint8_t offset, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
	{
		SegmentDescriptor& desc = *(SegmentDescriptor*)((uintptr_t)s_gdt + offset);
		desc.base1 = base;
		desc.base2 = base >> 16;
		desc.base3 = base >> 24;

		desc.limit1 = limit;
		desc.limit2 = limit >> 16;

		desc.access = access;

		desc.flags = flags;
	}

	static void write_tss()
	{
		s_tss = new TaskStateSegment();
		ASSERT(s_tss);
		
		memset(s_tss, 0x00, sizeof(TaskStateSegment));
		s_tss->rsp0 = 0;
		
		uintptr_t base = (uintptr_t)s_tss;

		write_entry(s_tss_offset, (uint32_t)base, sizeof(TaskStateSegment), 0x89, 0x0);
		SegmentDescriptor& desc = *(SegmentDescriptor*)((uintptr_t)s_gdt + s_tss_offset + 0x08);
		desc.low = base >> 32;
		desc.high = 0;
	}

	void set_tss_stack(uintptr_t rsp)
	{
		s_tss->rsp0 = rsp;
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
		constexpr uint32_t descriptor_count = 6 + 1; // tss takes 2
		s_gdt = new SegmentDescriptor[descriptor_count]; 
		ASSERT(s_gdt);

		s_gdtr.address = (uint64_t)s_gdt;
		s_gdtr.size = descriptor_count * sizeof(SegmentDescriptor) - 1;

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