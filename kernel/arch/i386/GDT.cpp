#include <BAN/Assert.h>
#include <kernel/GDT.h>

#include <string.h>

extern "C" uintptr_t g_boot_stack_top[0];

namespace Kernel::GDT
{

	struct TaskStateSegment
	{
		uint16_t link;
		uint16_t reserved1;
		uint32_t esp0;
		uint16_t ss0;
		uint16_t reserved2;
		uint32_t esp1;
		uint16_t ss1;
		uint16_t reserved3;
		uint32_t esp2;
		uint16_t ss2;
		uint16_t reserved4;
		uint32_t cr3;
		uint32_t eip;
		uint32_t eflags;
		uint32_t eax;
		uint32_t ecx;
		uint32_t edx;
		uint32_t ebx;
		uint32_t esp;
		uint32_t ebp;
		uint32_t esi;
		uint32_t edi;
		uint16_t es;
		uint16_t reserved5;
		uint16_t cs;
		uint16_t reserved6;
		uint16_t ss;
		uint16_t reserved7;
		uint16_t ds;
		uint16_t reserved8;
		uint16_t fs;
		uint16_t reserved9;
		uint16_t gs;
		uint16_t reserved10;
		uint16_t ldtr;
		uint16_t reserved11;
		uint16_t reserved12;
		uint16_t iopb;
		uint32_t ssp;
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
		uint32_t address;
	} __attribute__((packed));

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

	static void write_tss(uint8_t offset)
	{
		s_tss = new TaskStateSegment();
		ASSERT(s_tss);

		memset(s_tss, 0x00, sizeof(TaskStateSegment));
		s_tss->ss0 = 0x10;
		s_tss->esp0 = (uintptr_t)g_boot_stack_top;

		write_entry(offset, (uint32_t)s_tss, sizeof(TaskStateSegment), 0x89, 0x0);
	}

	void set_tss_stack(uintptr_t esp)
	{
		s_tss->esp0 = esp;
	}

	static void flush_gdt()
	{
		asm volatile("lgdt %0" :: "m"(s_gdtr));
	}

	extern "C" void flush_tss(uint16_t offset)
	{
		asm volatile("ltr %0" :: "m"(offset));
	}

	void initialize()
	{
		constexpr uint32_t descriptor_count = 6;
		s_gdt = new SegmentDescriptor[descriptor_count];
		ASSERT(s_gdt);

		s_gdtr.address = (uint64_t)s_gdt;
		s_gdtr.size = descriptor_count * sizeof(SegmentDescriptor) - 1;

		write_entry(0x00, 0x00000000, 0x00000, 0x00, 0x0); // null
		write_entry(0x08, 0x00000000, 0xFFFFF, 0x9A, 0xC); // kernel code
		write_entry(0x10, 0x00000000, 0xFFFFF, 0x92, 0xC); // kernel data
		write_entry(0x18, 0x00000000, 0xFFFFF, 0xFA, 0xC); // user code
		write_entry(0x20, 0x00000000, 0xFFFFF, 0xF2, 0xC); // user data
		write_tss(0x28);

		flush_gdt();
		flush_tss(0x28);
	}

}
