#include <BAN/ScopeGuard.h>
#include <kernel/ACPI.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2Controller.h>
#include <kernel/Input/PS2Keyboard.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>

namespace Kernel::Input
{

	namespace PS2
	{

		enum IOPort : uint8_t
		{
			DATA = 0x60,
			STATUS = 0x64,
			COMMAND = 0x64,
		};

		enum Status : uint8_t
		{
			OUTPUT_FULL = (1 << 0),
			INPUT_FULL = (1 << 1),
			SYSTEM = (1 << 2),
			DEVICE_OR_CONTROLLER = (1 << 3),
			TIMEOUT_ERROR = (1 << 6),
			PARITY_ERROR = (1 << 7),
		};

		enum Config : uint8_t
		{
			INTERRUPT_FIRST_PORT = (1 << 0),
			INTERRUPT_SECOND_PORT = (1 << 1),
			SYSTEM_FLAG = (1 << 2),
			ZERO1 = (1 << 3),
			CLOCK_FIRST_PORT = (1 << 4),
			CLOCK_SECOND_PORT = (1 << 5),
			TRANSLATION_FIRST_PORT = (1 << 6),
			ZERO2 = (1 << 7),
		};

		enum Command : uint8_t
		{
			READ_CONFIG = 0x20,
			WRITE_CONFIG = 0x60,
			DISABLE_SECOND_PORT = 0xA7,
			ENABLE_SECOND_PORT = 0xA8,
			TEST_SECOND_PORT = 0xA9,
			TEST_CONTROLLER = 0xAA,
			TEST_FIRST_PORT = 0xAB,
			DISABLE_FIRST_PORT = 0xAD,
			ENABLE_FIRST_PORT = 0xAE,
			WRITE_TO_SECOND_PORT = 0xD4,
		};

		enum Response
		{
			TEST_FIRST_PORT_PASS = 0x00,
			TEST_SECOND_PORT_PASS = 0x00,
			TEST_CONTROLLER_PASS = 0x55,
			SELF_TEST_PASS = 0xAA,
			ACK = 0xFA,
		};

		enum DeviceCommand
		{
			ENABLE_SCANNING = 0xF4,
			DISABLE_SCANNING = 0xF5,
			IDENTIFY = 0xF2,
			RESET = 0xFF,
		};

		enum IRQ
		{
			DEVICE0 = 1,
			DEVICE1 = 12,
		};

	}

	static constexpr uint64_t s_device_timeout_ms = 100;

	static void controller_send_command(PS2::Command command)
	{
		IO::outb(PS2::IOPort::COMMAND, command);
	}

	static void controller_send_command(PS2::Command command, uint8_t data)
	{
		IO::outb(PS2::IOPort::COMMAND, command);
		while (IO::inb(PS2::IOPort::STATUS) & PS2::Status::INPUT_FULL)
			continue;
		IO::outb(PS2::IOPort::DATA, data);
	}

	static uint8_t wait_and_read()
	{
		while (!(IO::inb(PS2::IOPort::STATUS) & PS2::Status::OUTPUT_FULL))
			continue;
		return IO::inb(PS2::IOPort::DATA);
	}

	static BAN::ErrorOr<void> device_send_byte(uint8_t device, uint8_t byte)
	{
		if (device == 1)
			IO::outb(PS2::IOPort::COMMAND, PS2::Command::WRITE_TO_SECOND_PORT);
		uint64_t timeout = PIT::ms_since_boot() + s_device_timeout_ms;
		while (PIT::ms_since_boot() < timeout)
		{
			if (!(IO::inb(PS2::IOPort::STATUS) & PS2::Status::INPUT_FULL))
			{
				IO::outb(PS2::IOPort::DATA, byte);
				return {};
			}
		}
		return BAN::Error::from_error_code(ErrorCode::PS2_Timeout);
	}

	static BAN::ErrorOr<uint8_t> device_read_byte()
	{
		uint64_t timeout = PIT::ms_since_boot() + s_device_timeout_ms;
		while (PIT::ms_since_boot() < timeout)
			if (IO::inb(PS2::IOPort::STATUS) & PS2::Status::OUTPUT_FULL)
				return IO::inb(PS2::IOPort::DATA);
		return BAN::Error::from_error_code(ErrorCode::PS2_Timeout);
	}

	static BAN::ErrorOr<void> device_wait_ack()
	{
		while (TRY(device_read_byte()) != PS2::Response::ACK)
			continue;;
		return {};
	}

	void PS2Controller::device0_irq()
	{
		auto& controller = PS2Controller::get();
		ASSERT(controller.m_devices[0] != nullptr);
		controller.m_devices[0]->on_byte(IO::inb(PS2::IOPort::DATA));
	}

	void PS2Controller::device1_irq()
	{
		auto& controller = PS2Controller::get();
		ASSERT(controller.m_devices[1] != nullptr);
		controller.m_devices[1]->on_byte(IO::inb(PS2::IOPort::DATA));
	}

	static PS2Controller* s_instance = nullptr;

	BAN::ErrorOr<void> PS2Controller::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new PS2Controller;
		if (s_instance == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([] { delete s_instance; });
		TRY(s_instance->initialize_impl());
		guard.disable();
		return {};
	}

	PS2Controller& PS2Controller::get()
	{
		ASSERT(s_instance != nullptr);
		return *s_instance;
	}

	BAN::ErrorOr<void> PS2Controller::initialize_impl()
	{
		// Step 1: Initialise USB Controllers
		// FIXME

		// Step 2: Determine if the PS/2 Controller Exists
		// FIXME

		// Step 3: Disable Devices
		controller_send_command(PS2::Command::DISABLE_FIRST_PORT);
		controller_send_command(PS2::Command::DISABLE_SECOND_PORT);

		// Step 4: Flush The Output Buffer
		IO::inb(PS2::IOPort::DATA);
		
		// Step 5: Set the Controller Configuration Byte
		controller_send_command(PS2::Command::READ_CONFIG);
		uint8_t config = wait_and_read();
		config &= ~PS2::Config::INTERRUPT_FIRST_PORT;
		config &= ~PS2::Config::INTERRUPT_SECOND_PORT;
		config &= ~PS2::Config::TRANSLATION_FIRST_PORT;
		controller_send_command(PS2::Command::WRITE_CONFIG, config);

		// Step 6: Perform Controller Self Test
		controller_send_command(PS2::Command::TEST_CONTROLLER);
		if (wait_and_read() != PS2::Response::TEST_CONTROLLER_PASS)
			return BAN::Error::from_error_code(ErrorCode::PS2_SelfTest);
		// NOTE: self test might reset the device so we set the config byte again
		controller_send_command(PS2::Command::WRITE_CONFIG, config);

		// Step 7: Determine If There Are 2 Channels
		bool valid_ports[2] { true, false };
		if (config & PS2::Config::CLOCK_SECOND_PORT)
		{
			controller_send_command(PS2::Command::ENABLE_SECOND_PORT);
			controller_send_command(PS2::Command::READ_CONFIG);
			if (!(wait_and_read() & PS2::Config::CLOCK_SECOND_PORT))
				valid_ports[1] = true;
			controller_send_command(PS2::Command::DISABLE_SECOND_PORT);
		}

		// Step 8: Perform Interface Tests
		controller_send_command(PS2::Command::TEST_FIRST_PORT);
		if (wait_and_read() != PS2::Response::TEST_FIRST_PORT_PASS)
			valid_ports[0] = false;
		if (valid_ports[1])
		{
			controller_send_command(PS2::Command::TEST_SECOND_PORT);
			if (wait_and_read() != PS2::Response::TEST_SECOND_PORT_PASS)
				valid_ports[1] = false;
		}
		if (!valid_ports[0] && !valid_ports[1])
			return {};
		
		// Step 9: Enable Devices (and disable scanning)
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			controller_send_command(device == 0 ? PS2::Command::ENABLE_FIRST_PORT : PS2::Command::ENABLE_SECOND_PORT);
			if (set_scanning(device, false).is_error())
				valid_ports[device] = false;
		}

		// Step 10: Reset Devices
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			if (reset_device(device).is_error())
				valid_ports[device] = false;
			if (set_scanning(device, false).is_error())
				valid_ports[device] = false;
		}

		// Step 11: Initialize Device Drivers
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			if (auto res = initialize_device(device); res.is_error())
				dprintln("{}", res.error());
		}

		if (m_devices[0])
		{
			IDT::register_irq_handler(PS2::IRQ::DEVICE0, device0_irq);
			InterruptController::get().enable_irq(PS2::IRQ::DEVICE0);
			config |= PS2::Config::INTERRUPT_FIRST_PORT;
			DevFileSystem::get().add_device("input0", m_devices[0]);
		}
		if (m_devices[1])
		{
			IDT::register_irq_handler(PS2::IRQ::DEVICE1, device1_irq);
			InterruptController::get().enable_irq(PS2::IRQ::DEVICE1);
			config |= PS2::Config::INTERRUPT_SECOND_PORT;
			DevFileSystem::get().add_device("input1", m_devices[1]);
		}

		controller_send_command(PS2::Command::WRITE_CONFIG, config);
			
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::initialize_device(uint8_t device)
	{
		TRY(device_send_byte(device, PS2::DeviceCommand::IDENTIFY));
		TRY(device_wait_ack());

		uint8_t bytes[2] {};
		uint8_t index = 0;
		for (uint8_t i = 0; i < 2; i++)
		{
			auto res = device_read_byte();
			if (res.is_error())
				break;
			bytes[index++] = res.value();
		}

		// Standard PS/2 Mouse
		if (index == 1 && (bytes[0] == 0x00))
			return BAN::Error::from_error_code(ErrorCode::PS2_UnsupportedDevice);
		
		// MF2 Keyboard
		if (index == 2 && (bytes[0] == 0xAB && bytes[1] == 0x83))
			m_devices[device] = TRY(PS2Keyboard::create(*this));

		if (m_devices[device])
			return {};

		return BAN::Error::from_error_code(ErrorCode::PS2_UnsupportedDevice);
	}

	void PS2Controller::send_byte(const PS2Device* device, uint8_t byte)
	{
		ASSERT(device != nullptr && (device == m_devices[0] || device == m_devices[1]));
		uint8_t device_index = (device == m_devices[0]) ? 0 : 1;
		MUST(device_send_byte(device_index, byte));
	}

	BAN::ErrorOr<void> PS2Controller::reset_device(uint8_t device)
	{
		TRY(device_send_byte(device, PS2::DeviceCommand::RESET));
		TRY(device_wait_ack());
		if (TRY(device_read_byte()) != PS2::Response::SELF_TEST_PASS)
			return BAN::Error::from_error_code(ErrorCode::PS2_Reset);
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::set_scanning(uint8_t device, bool enabled)
	{
		TRY(device_send_byte(device, enabled ? PS2::DeviceCommand::ENABLE_SCANNING : PS2::DeviceCommand::DISABLE_SCANNING));
		TRY(device_wait_ack());
		return {};
	}

}