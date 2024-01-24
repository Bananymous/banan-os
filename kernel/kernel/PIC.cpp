#include <kernel/CriticalScope.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/PIC.h>

#include <string.h>

#define PIC1_CMD	0x20
#define PIC1_DATA	0x21
#define PIC2_CMD	0xA0
#define PIC2_DATA	0xA1

#define PIC_ISR		0x0B
#define PIC_EOI		0x20

#define ICW1_ICW4	0x01
#define ICW1_INIT	0x10

#define ICW4_8086	0x01

namespace Kernel
{

	PIC* PIC::create()
	{
		mask_all();
		remap();
		return new PIC;
	}

	void PIC::remap()
	{
		uint8_t a1 = IO::inb(PIC1_DATA);
		uint8_t a2 = IO::inb(PIC2_DATA);

		// Start the initialization sequence (in cascade mode)
		IO::outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
		IO::io_wait();
		IO::outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
		IO::io_wait();

		// ICW2
		IO::outb(PIC1_DATA, IRQ_VECTOR_BASE);
		IO::io_wait();
		IO::outb(PIC2_DATA, IRQ_VECTOR_BASE + 0x08);
		IO::io_wait();

		// ICW3
		IO::outb(PIC1_DATA, 4);
		IO::io_wait();
		IO::outb(PIC2_DATA, 2);
		IO::io_wait();

		// ICW4
		IO::outb(PIC1_DATA, ICW4_8086);
		IO::io_wait();
		IO::outb(PIC2_DATA, ICW4_8086);
		IO::io_wait();

		// Restore original masks
		IO::outb(PIC1_DATA, a1);
		IO::outb(PIC2_DATA, a2);
	}

	void PIC::mask_all()
	{
		// NOTE: don't mask irq 2 as it is needed for slave pic
		IO::outb(PIC1_DATA, 0xFB);
		IO::outb(PIC2_DATA, 0xFF);
	}

	void PIC::eoi(uint8_t irq)
	{
		ASSERT(!interrupts_enabled());
		if (irq >= 8)
			IO::outb(PIC2_CMD, PIC_EOI);
		IO::outb(PIC1_CMD, PIC_EOI);
	}

	void PIC::enable_irq(uint8_t irq)
	{
		CriticalScope _;
		ASSERT(irq < 16);
		ASSERT(m_reserved_irqs & (1 << irq));

		uint16_t port = PIC1_DATA;
		if(irq >= 8)
		{
			port = PIC2_DATA;
			irq -= 8;
		}
		IO::outb(port, IO::inb(port) & ~(1 << irq));
	}

	BAN::ErrorOr<void> PIC::reserve_irq(uint8_t irq)
	{
		if (irq >= 16)
		{
			dwarnln("PIC only supports 16 irqs");
			return BAN::Error::from_errno(EFAULT);
		}
		CriticalScope _;
		if (m_reserved_irqs & (1 << irq))
		{
			dwarnln("irq {} is already reserved", irq);
			return BAN::Error::from_errno(EFAULT);
		}
		m_reserved_irqs |= 1 << irq;
		return {};
	}

	BAN::Optional<uint8_t> PIC::get_free_irq()
	{
		CriticalScope _;
		for (int irq = 0; irq < 16; irq++)
		{
			if (m_reserved_irqs & (1 << irq))
				continue;
			m_reserved_irqs |= 1 << irq;
			return irq;
		}

		return {};
	}

	bool PIC::is_in_service(uint8_t irq)
	{
		uint16_t port = PIC1_CMD;
		if (irq >= 8)
		{
			port = PIC2_CMD;
			irq -= 8;
		}
		IO::outb(port, PIC_ISR);
		return IO::inb(port) & (1 << irq);
	}

}
