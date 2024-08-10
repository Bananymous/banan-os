#include <BAN/Array.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Terminal/Serial.h>

#include <ctype.h>

#define MAX_BAUD 115200

#define DATA_BITS_5		(0b00 << 0)
#define DATA_BITS_6		(0b01 << 0)
#define DATA_BITS_7		(0b10 << 0)
#define DATA_BITS_8		(0b11 << 0)

#define STOP_BITS_1		(0b0 << 2)
#define STOP_BITS_2		(0b1 << 2)

#define PARITY_NONE		(0b000 << 3)
#define PARITY_ODD		(0b001 << 3)
#define PARITY_EVEN		(0b011 << 3)
#define PARITY_MARK		(0b101 << 3)
#define PARITY_SPACE	(0b111 << 3)

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
#define COM1_IRQ 4
#define COM2_IRQ 3

namespace Kernel
{

	static BAN::Atomic<uint32_t> s_next_tty_number = 0;

	static constexpr uint16_t s_serial_ports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8, 0x5F8, 0x4F8, 0x5E8, 0x4E8 };
	static BAN::Array<Serial, sizeof(s_serial_ports) / sizeof(*s_serial_ports)> s_serial_drivers;
	static bool s_has_devices { false };

	static BAN::RefPtr<SerialTTY> s_com1;
	static BAN::RefPtr<SerialTTY> s_com2;

	void Serial::initialize()
	{
		int count = 0;
		for (size_t i = 0; i < s_serial_drivers.size(); i++)
		{
			if (initialize_port(s_serial_ports[i], 115200))
			{
				auto& driver = s_serial_drivers[i];
				driver.m_port = s_serial_ports[i];
				if (!driver.initialize_size())
				{
					// if size detection fails, just use some random size
					driver.m_width	= 999;
					driver.m_height	= 999;
				}
				count++;
			}
		}
		s_has_devices = !!count;

		for (auto& driver : s_serial_drivers)
			if (driver.is_valid())
				dprintln("{}x{} serial device at 0x{H}", driver.width(), driver.height(), driver.port());
	}

	void Serial::initialize_devices()
	{
		for (auto& serial : s_serial_drivers)
			if (serial.is_valid())
				MUST(SerialTTY::create(serial));
	}

	bool Serial::initialize_port(uint16_t port, uint32_t baud)
	{
		// Disable interrupts
		IO::outb(port + 1, 0x00);

		// configure port
		uint16_t divisor = MAX_BAUD / baud;
		IO::outb(port + 3, 0x80);
		IO::outb(port + 0, divisor & 0xFF);
		IO::outb(port + 1, divisor >> 8);
		IO::outb(port + 3, DATA_BITS_8 | STOP_BITS_1 | PARITY_NONE);

		IO::outb(port + 2, 0xC7);
		IO::outb(port + 4, 0x0B);

		// Test loopback
		IO::outb(port + 4, 0x1E);
		IO::outb(port + 0, 0xAE);
		if(IO::inb(port + 0) != 0xAE)
			return false;

		// Set to normal mode
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
		: TTY(0600, 0, 0)
		, m_name(MUST(BAN::String::formatted("ttyS{}", s_next_tty_number++)))
		, m_serial(serial)
	{}

	BAN::ErrorOr<BAN::RefPtr<SerialTTY>> SerialTTY::create(Serial serial)
	{
		auto* tty = new SerialTTY(serial);
		ASSERT(tty);

		// Enable interrupts for COM1 and COM2
		if (serial.port() == COM1_PORT)
		{
			IO::outb(COM1_PORT + 1, 1);
			TRY(InterruptController::get().reserve_irq(COM1_IRQ));
			tty->set_irq(COM1_IRQ);
			tty->enable_interrupt();
		}
		else if (serial.port() == COM2_PORT)
		{
			IO::outb(COM2_PORT + 1, 1);
			TRY(InterruptController::get().reserve_irq(COM2_IRQ));
			tty->set_irq(COM2_IRQ);
			tty->enable_interrupt();
		}

		auto ref_ptr = BAN::RefPtr<SerialTTY>::adopt(tty);
		DevFileSystem::get().add_device(ref_ptr);
		if (serial.port() == COM1_PORT)
			s_com1 = ref_ptr;
		if (serial.port() == COM2_PORT)
			s_com2 = ref_ptr;
		return ref_ptr;
	}

	void SerialTTY::handle_irq()
	{
		uint8_t ch = IO::inb(m_serial.port());

		SpinLockGuard _(m_input_lock);
		if (m_input.full())
		{
			dwarnln("Serial buffer full");
			m_input.pop();
		}
		m_input.push(ch);
	}

	void SerialTTY::update()
	{
		if (m_serial.port() != COM1_PORT && m_serial.port() != COM2_PORT)
			return;

		uint8_t buffer[128];

		{
			SpinLockGuard _(m_input_lock);
			if (m_input.empty())
				return;
			uint8_t* ptr = buffer;
			while (!m_input.empty())
			{
				*ptr = m_input.front();
				if (*ptr == '\r')
					*ptr = '\n';
				if (*ptr == 127)
					*ptr++ = '\b', *ptr++ = ' ', *ptr = '\b';
				m_input.pop();
				ptr++;
			}
			*ptr = '\0';
		}

		const uint8_t* ptr = buffer;
		while (*ptr)
			handle_input_byte(*ptr++);
	}

	uint32_t SerialTTY::width() const
	{
		return m_serial.width();
	}

	uint32_t SerialTTY::height() const
	{
		return m_serial.height();
	}

	void SerialTTY::putchar_impl(uint8_t ch)
	{
		m_serial.putchar(ch);
	}

}
