#include <kernel/GDT.h>
#include <kernel/Debug.h>

#include <string.h>

namespace Kernel
{

	GDT* GDT::create()
	{
		auto* gdt = new GDT();
		ASSERT(gdt);

		gdt->write_entry(0x00, 0x00000000, 0x00000, 0x00, 0x0); // null
		gdt->write_entry(0x08, 0x00000000, 0xFFFFF, 0x9A, 0xA); // kernel code
		gdt->write_entry(0x10, 0x00000000, 0xFFFFF, 0x92, 0xC); // kernel data
		gdt->write_entry(0x18, 0x00000000, 0xFFFFF, 0xFA, 0xA); // user code
		gdt->write_entry(0x20, 0x00000000, 0xFFFFF, 0xF2, 0xC); // user data
		gdt->write_tss();

		return gdt;
	}

	void GDT::write_entry(uint8_t offset, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
	{
		ASSERT(offset % sizeof(SegmentDescriptor) == 0);
		uint8_t idx = offset / sizeof(SegmentDescriptor);

		auto& desc = m_gdt[idx];
		desc.base1 = (base >>  0) & 0xFFFF;
		desc.base2 = (base >> 16) & 0xFF;
		desc.base3 = (base >> 24) & 0xFF;

		desc.limit1 = (limit >>  0) & 0xFFFF;
		desc.limit2 = (limit >> 16) & 0x0F;

		desc.access = access & 0xFF;

		desc.flags = flags & 0x0F;
	}

	void GDT::write_tss()
	{
		memset(&m_tss, 0x00, sizeof(TaskStateSegment));
		m_tss.iopb = sizeof(TaskStateSegment);

		uint64_t base = reinterpret_cast<uint64_t>(&m_tss);

		write_entry(0x28, (uint32_t)base, sizeof(TaskStateSegment), 0x89, 0x0);

		auto& desc = m_gdt[0x30 / sizeof(SegmentDescriptor)];
		desc.low = base >> 32;
		desc.high = 0;
	}

}
