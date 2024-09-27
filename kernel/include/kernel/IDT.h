#pragma once

#include <BAN/Array.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Arch.h>
#include <kernel/Interruptable.h>

#include <stdint.h>

namespace Kernel
{

	// IDT entries
	//   0x00->0x1F (32):  ISR
	//   0x20->0x7F (96):  PIC/IOAPIC
	//   0x80->0xEF (112): MSI
	//   0xF0->0xFE (15):  internal

	constexpr uint8_t IRQ_VECTOR_BASE   = 0x20;
	constexpr uint8_t IRQ_MSI_BASE      = 0x80;
	constexpr uint8_t IRQ_SYSCALL       = 0xF0;
	constexpr uint8_t IRQ_YIELD         = 0xF1;
	constexpr uint8_t IRQ_IPI           = 0xF2;
	constexpr uint8_t IRQ_TIMER         = 0xF3;

#if ARCH(x86_64)
	struct GateDescriptor
	{
		uint16_t offset0;
		uint16_t selector;
		uint8_t IST;
		uint8_t flags;
		uint16_t offset1;
		uint32_t offset2;
		uint32_t reserved;
	};
	static_assert(sizeof(GateDescriptor) == 16);
#elif ARCH(i686)
	struct GateDescriptor
	{
		uint16_t offset0;
		uint16_t selector;
		uint8_t reserved;
		uint8_t flags;
		uint16_t offset1;
	};
	static_assert(sizeof(GateDescriptor) == 8);
#else
	#error
#endif

	struct IDTR
	{
		uint16_t size;
		uintptr_t offset;
	} __attribute__((packed));

	class IDT
	{
		BAN_NON_COPYABLE(IDT);
		BAN_NON_MOVABLE(IDT);

	public:
		static IDT* create();

		[[noreturn]] static void force_triple_fault();

		void register_irq_handler(uint8_t irq, Interruptable* interruptable);

		void load()
		{
			asm volatile("lidt %0" :: "m"(m_idtr) : "memory");
		}

	private:
		IDT() = default;

		void register_interrupt_handler(uint8_t index, void (*handler)());
		void register_syscall_handler(uint8_t index, void (*handler)());

	private:
		BAN::Array<GateDescriptor, 0x100> m_idt;
		IDTR m_idtr {
			.size = static_cast<uint16_t>(m_idt.size() * sizeof(GateDescriptor) - 1),
			.offset = reinterpret_cast<uintptr_t>(m_idt.data())
		};
	};

}
