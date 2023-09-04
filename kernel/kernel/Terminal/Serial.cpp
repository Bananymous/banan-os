#include <BAN/Array.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IO.h>
#include <kernel/Terminal/Serial.h>

#include <ctype.h>
#include <sys/sysmacros.h>

namespace Kernel
{

	static constexpr uint16_t s_serial_ports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8, 0x5F8, 0x4F8, 0x5E8, 0x4E8 };
	static BAN::Array<Serial, sizeof(s_serial_ports) / sizeof(*s_serial_ports)> s_serial_drivers;
	static bool s_has_devices { false };


	static dev_t next_rdev()
	{
		static dev_t major = DevFileSystem::get().get_next_dev();
		static dev_t minor = 0;
		return makedev(major, minor++);
	}

	void Serial::initialize()
	{
		int count = 0;
		for (size_t i = 0; i < s_serial_drivers.size(); i++)
		{
			if (port_has_device(s_serial_ports[i]))
			{
				auto& driver = s_serial_drivers[i];
				driver.m_port = s_serial_ports[i];
				if (!driver.initialize_size())
					continue;
				count++;
			}
		}
		s_has_devices = !!count;

		for (auto& driver : s_serial_drivers)
			dprintln("{}x{} serial device at 0x{H}", driver.width(), driver.height(), driver.port());
	}

	void Serial::initialize_devices()
	{
		for (auto& serial : s_serial_drivers)
			if (serial.is_valid())
				MUST(SerialTTY::create(serial));
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
		if(IO::inb(port + 0) != 0xAE)
			return false;

		// If serial is not faulty set it in normal operation mode
		// (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
		IO::outb(port + 4, 0x0F);

		return true;
	}

	bool Serial::initialize_size()
	{
		const char* query = "\e[999;999H\e[6n\e[H\e[J";

		const char* ptr = query;
		while (*ptr)
			putchar(*ptr++);
		
		if (getchar() != '\033')
			return false;
		if (getchar() != '[')
			return false;

		auto read_number =
			[&](char end)
			{
				uint32_t number = 0;
				while (true)
				{
					char c = getchar();
					if (c == end)
						break;
					if (!isdigit(c))
						return UINT32_MAX;
					number = (number * 10) + (c - '0');
				}
				return number;
			};

		m_height = read_number(';');
		if (m_height == UINT32_MAX)
		{
			m_port = 0;
			return false;
		}

		m_width = read_number('R');
		if (m_width == UINT32_MAX)
		{
			m_port = 0;
			return false;
		}

		return true;
	}

	bool Serial::has_devices()
	{
		return s_has_devices;
	}
 
	void Serial::putchar(char c)
	{
		while (!(IO::inb(m_port + 5) & 0x20))
			continue;
		IO::outb(m_port, c);
	}

	char Serial::getchar()
	{
		while (!(IO::inb(m_port + 5) & 0x01))
			continue;
		return IO::inb(m_port);
	}

	void Serial::putchar_any(char c)
	{
		for (auto& device : s_serial_drivers)
			if (device.is_valid())
				device.putchar(c);
	}

	SerialTTY::SerialTTY(Serial serial)
		: TTY(0660, 0, 0)
		, m_serial(serial)
		, m_rdev(next_rdev())
	{}

	BAN::ErrorOr<BAN::RefPtr<SerialTTY>> SerialTTY::create(Serial serial)
	{
		auto* tty = new SerialTTY(serial);
		ASSERT(tty);

		ASSERT(minor(tty->rdev()) < 10);
		char name[] = { 't', 't', 'y', 'S', (char)('0' + minor(tty->rdev())), '\0' };
		
		auto ref_ptr = BAN::RefPtr<SerialTTY>::adopt(tty);
		DevFileSystem::get().add_device(name, ref_ptr);
		return ref_ptr;
	}
	
	uint32_t SerialTTY::width() const
	{
		return m_serial.width();
	}

	uint32_t SerialTTY::height() const
	{
		return m_serial.height();
	}
	
	void SerialTTY::putchar(uint8_t ch)
	{
		m_serial.putchar(ch);
	}

}