#pragma once

#include <BAN/Array.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Arch.h>

#include <stdint.h>

namespace Kernel
{

#if ARCH(x86_64)
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
	static_assert(sizeof(TaskStateSegment) == 104);
#elif ARCH(i686)
	struct TaskStateSegment
	{
		uint16_t link;
		uint16_t __reserved0;
		uint32_t esp0;
		uint16_t ss0;
		uint16_t __reserved1;
		uint32_t esp1;
		uint16_t ss1;
		uint16_t __reserved2;
		uint32_t esp2;
		uint16_t ss2;
		uint16_t __reserved3;
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
		uint16_t __reserved4;
		uint16_t cs;
		uint16_t __reserved5;
		uint16_t ss;
		uint16_t __reserved6;
		uint16_t ds;
		uint16_t __reserved7;
		uint16_t fs;
		uint16_t __reserved8;
		uint16_t gs;
		uint16_t __reserved9;
		uint16_t ldtr;
		uint16_t __reserved10;
		uint16_t __reserved11;
		uint16_t iopb;
		uint32_t ssp;
	};
	static_assert(sizeof(TaskStateSegment) == 108);
#else
	#error
#endif

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
		};

		struct
		{
			uint32_t low;
			uint32_t high;
		};
	};
	static_assert(sizeof(SegmentDescriptor) == 8);

	struct GDTR
	{
		uint16_t size;
		uintptr_t address;
	} __attribute__((packed));

	class GDT
	{
		BAN_NON_COPYABLE(GDT);
		BAN_NON_MOVABLE(GDT);

	public:
		static GDT* create(void* processor);
		void load() { flush_gdt(); flush_tss(); }

		static constexpr inline bool is_user_segment(uint8_t segment)
		{
			return (segment & 3) == 3;
		}

		void set_tss_stack(uintptr_t sp)
		{
#if ARCH(x86_64)
			m_tss.rsp0 = sp;
#elif ARCH(i686)
			m_tss.esp0 = sp;
			m_tss.ss0 = 0x10;
#endif
		}

#if ARCH(i686)
		void set_tls(uintptr_t addr);
#endif

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
			asm volatile("ltr %0" :: "rm"(m_tss_offset) : "memory");
		}

	private:
#if ARCH(x86_64)
		BAN::Array<SegmentDescriptor, 7> m_gdt; // null, kernel code, kernel data, user code, user data, tss low, tss high
		static constexpr uint16_t m_tss_offset = 0x28;
#elif ARCH(i686)
		BAN::Array<SegmentDescriptor, 8> m_gdt; // null, kernel code, kernel data, user code, user data, processor data, tls, tss
		static constexpr uint16_t m_tss_offset = 0x38;
#endif
		TaskStateSegment m_tss;
		const GDTR m_gdtr {
			.size = static_cast<uint16_t>(m_gdt.size() * sizeof(SegmentDescriptor) - 1),
			.address = reinterpret_cast<uintptr_t>(m_gdt.data())
		};
	};

}
