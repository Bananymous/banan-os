#include <kernel/GDT.h>
#include <kernel/Processor.h>

#include <string.h>

namespace Kernel
{

	GDT* GDT::create([[maybe_unused]] void* processor)
	{
		auto* gdt = new GDT();
		ASSERT(gdt);

#if ARCH(x86_64)
		constexpr uint8_t code_flags = 0xA;
		constexpr uint8_t data_flags = 0xC;
#elif ARCH(i686)
		constexpr uint8_t code_flags = 0xC;
		constexpr uint8_t data_flags = 0xC;
#endif

		gdt->write_entry(0x00, 0x00000000, 0x00000, 0x00, 0x0);			// null
		gdt->write_entry(0x08, 0x00000000, 0xFFFFF, 0x9A, code_flags);	// kernel code
		gdt->write_entry(0x10, 0x00000000, 0xFFFFF, 0x92, data_flags);	// kernel data
		gdt->write_entry(0x18, 0x00000000, 0xFFFFF, 0xFA, code_flags);	// user code
		gdt->write_entry(0x20, 0x00000000, 0xFFFFF, 0xF2, data_flags);	// user data
#if ARCH(i686)
		gdt->write_entry(0x28, reinterpret_cast<uint32_t>(processor), sizeof(Processor), 0x92, 0x4); // processor data
#endif
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

		uintptr_t base = reinterpret_cast<uintptr_t>(&m_tss);

		write_entry(m_tss_offset, (uint32_t)base, sizeof(TaskStateSegment), 0x89, 0x0);

#if ARCH(x86_64)
		auto& desc = m_gdt[(m_tss_offset + 8) / sizeof(SegmentDescriptor)];
		desc.low = base >> 32;
		desc.high = 0;
#endif
	}

}
