#pragma once

#include <BAN/Array.h>
#include <BAN/NoCopyMove.h>

#include <stdint.h>

namespace Kernel
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

	class GDT
	{
		BAN_NON_COPYABLE(GDT);
		BAN_NON_MOVABLE(GDT);

	public:
		static GDT* create();
		void load() { flush_gdt(); flush_tss(); }

		static constexpr inline bool is_user_segment(uint8_t segment)
		{
			return (segment & 3) == 3;
		}

		void set_tss_stack(uintptr_t rsp)
		{
			m_tss.rsp0 = rsp;
		}

	private:
		GDT() = default;

		void write_entry(uint8_t offset, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
		void write_tss();

		void flush_gdt()
		{
			asm volatile("lgdt %0" :: "m"(m_gdtr) : "memory");
		}

		void flush_tss()
		{
			asm volatile("ltr %0" :: "rm"((uint16_t)0x28) : "memory");
		}

	private:
		BAN::Array<SegmentDescriptor, 7> m_gdt; // null, kernel code, kernel data, user code, user data, tss low, tss high
		TaskStateSegment m_tss;
		const GDTR m_gdtr {
			.size = m_gdt.size() * sizeof(SegmentDescriptor) - 1,
			.address = reinterpret_cast<uint64_t>(m_gdt.data())
		};
	};

}
