#include <kernel/PIC.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>

#define PIC1			0x20		/* IO base address for master PIC */
#define PIC2			0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA		(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA		(PIC2+1)

#define PIC_EOI			0x20		/* End-of-interrupt command code */

#define ICW1_ICW4		0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE		0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL		0x08		/* Level triggered (edge) mode */
#define ICW1_INIT		0x10		/* Initialization - required! */
 
#define ICW4_8086		0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO		0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM		0x10		/* Special fully nested (not) */


namespace PIC
{

	void Remap()
	{
		uint8_t a1 = IO::inb(PIC1_DATA);
		uint8_t a2 = IO::inb(PIC2_DATA);

		// Start the initialization sequence (in cascade mode)
		IO::outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
		IO::io_wait();
		IO::outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
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

	void MaskAll()
	{
		IO::outb(PIC1_DATA, 0xff);
		IO::outb(PIC2_DATA, 0xff);
	}

	void EOI(uint8_t irq)
	{
		if (irq >= 8)
			IO::outb(PIC2_COMMAND, PIC_EOI);
		IO::outb(PIC1_COMMAND, PIC_EOI);
	}

	void Mask(uint8_t irq) {
		uint16_t port;
		uint8_t value;
	
		if(irq < 8) {
			port = PIC1_DATA;
		} else {
			port = PIC2_DATA;
			irq -= 8;
		}
		value = IO::inb(port) | (1 << irq);
		IO::outb(port, value);        
	}
	
	void Unmask(uint8_t irq) {
		uint16_t port;
		uint8_t value;
	
		if(irq < 8) {
			port = PIC1_DATA;
		} else {
			port = PIC2_DATA;
			irq -= 8;
		}
		value = IO::inb(port) & ~(1 << irq);
		IO::outb(port, value);        
	}

	uint16_t GetISR()
	{
		IO::outb(PIC1_COMMAND, 0x0b);
		IO::outb(PIC2_COMMAND, 0x0b);
		uint8_t isr0 = IO::inb(PIC1_COMMAND);
		uint8_t isr1 = IO::inb(PIC2_COMMAND);
		return (isr1 << 8) | isr0;
	}

}