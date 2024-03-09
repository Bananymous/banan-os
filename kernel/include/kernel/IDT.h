#pragma once

#include <BAN/Array.h>
#include <BAN/NoCopyMove.h>
#include <kernel/Interruptable.h>

#include <stdint.h>

constexpr uint8_t IRQ_VECTOR_BASE = 0x20;
constexpr uint8_t IRQ_IPI = 32;

namespace Kernel
{

	struct GateDescriptor
	{
		uint16_t offset1;
		uint16_t selector;
		uint8_t IST;
		uint8_t flags;
		uint16_t offset2;
		uint32_t offset3;
		uint32_t reserved;
	} __attribute__((packed));

	struct IDTR
	{
		uint16_t size;
		uint64_t offset;
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
			.size = m_idt.size() * sizeof(GateDescriptor) - 1,
			.offset = reinterpret_cast<uint64_t>(m_idt.data())
		};
	};

}
