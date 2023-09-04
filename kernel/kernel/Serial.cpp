#include <BAN/Array.h>
#include <kernel/IO.h>
#include <kernel/Serial.h>

#define COM1_PORT 0x3f8

namespace Kernel
{

	static constexpr uint16_t s_serial_ports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8, 0x5F8, 0x4F8, 0x5E8, 0x4E8 };
	static BAN::Array<Serial, sizeof(s_serial_ports) / sizeof(*s_serial_ports)> s_serial_devices;
	static bool s_has_devices { false };

	void Serial::initialize()
	{
		int count = 0;
		for (size_t i = 0; i < s_serial_devices.size(); i++)
		{
			if (port_has_device(s_serial_ports[i]))
			{
				s_serial_devices[i].m_port = s_serial_ports[i];
				count++;
			}
		}
		s_has_devices = !!count;
		dprintln("Initialized {} serial devices", count);
	}

	bool Serial::port_has_device(uint16_t port)
	{
		IO::outb(port + 1, 0x00);	// Disable all interrupts
		IO::outb(port + 3, 0x80);	// Enable DLAB (set baud rate divisor)
		IO::outb(port + 0, 0x03);	// Set divisor to 3 (lo byte) 38400 baud
		IO::outb(port + 1, 0x00);	//                  (hi byte)
		IO::outb(port + 3, 0x03);	// 8 bits, no parity, one stop bit
		IO::outb(port + 2, 0xC7);	// Enable FIFO, clear them, with 14-byte threshold
		IO::outb(port + 4, 0x0B);	// IRQs enabled, RTS/DSR set
		IO::outb(port + 4, 0x1E);	// Set in loopback mode, test the serial chip
		IO::outb(port + 0, 0xAE);	// Test serial chip (send byte 0xAE and check if serial returns same byte)

		// Check if serial is faulty (i.e: not same byte as sent)
		if(IO::inb(COM1_PORT + 0) != 0xAE)
			return false;

		// If serial is not faulty set it in normal operation mode
		// (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
		IO::outb(port + 4, 0x0F);

		return true;
	}

	bool Serial::has_devices()
	{
		return s_has_devices;
	}

	bool Serial::is_transmit_empty() const
	{
		return !(IO::inb(m_port + 5) & 0x20);
	}
 
	void Serial::putchar(char c)
	{
		while (is_transmit_empty())
			continue;
		IO::outb(m_port, c);
	}

	void Serial::putchar_any(char c)
	{
		for (auto& device : s_serial_devices)
			if (device.is_valid())
				device.putchar(c);
	}

}