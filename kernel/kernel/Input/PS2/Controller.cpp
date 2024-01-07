#include <BAN/ScopeGuard.h>
#include <kernel/ACPI.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Controller.h>
#include <kernel/Input/PS2/Keyboard.h>
#include <kernel/Input/PS2/Mouse.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Timer/Timer.h>

#define DEBUG_PS2 1

namespace Kernel::Input
{

	static constexpr uint64_t s_ps2_timeout_ms = 100;

	static PS2Controller* s_instance = nullptr;

	BAN::ErrorOr<void> PS2Controller::send_byte(uint16_t port, uint8_t byte)
	{
		LockGuard _(m_lock);
		uint64_t timeout = SystemTimer::get().ms_since_boot() + s_ps2_timeout_ms;
		while (SystemTimer::get().ms_since_boot() < timeout)
		{
			if (IO::inb(PS2::IOPort::STATUS) & PS2::Status::INPUT_STATUS)
				continue;
			IO::outb(port, byte);
			return {};
		}
		return BAN::Error::from_errno(ETIMEDOUT);
	}

	BAN::ErrorOr<uint8_t> PS2Controller::read_byte()
	{
		LockGuard _(m_lock);
		uint64_t timeout = SystemTimer::get().ms_since_boot() + s_ps2_timeout_ms;
		while (SystemTimer::get().ms_since_boot() < timeout)
		{
			if (!(IO::inb(PS2::IOPort::STATUS) & PS2::Status::OUTPUT_STATUS))
				continue;
			return IO::inb(PS2::IOPort::DATA);
		}
		return BAN::Error::from_errno(ETIMEDOUT);
	}

	BAN::ErrorOr<void> PS2Controller::send_command(PS2::Command command)
	{
		LockGuard _(m_lock);
		TRY(send_byte(PS2::IOPort::COMMAND, command));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::send_command(PS2::Command command, uint8_t data)
	{
		LockGuard _(m_lock);
		TRY(send_byte(PS2::IOPort::COMMAND, command));
		TRY(send_byte(PS2::IOPort::DATA, data));
		return {};
	}

	bool PS2Controller::lock_command(PS2Device* device)
	{
		if (m_executing_device && m_executing_device != device)
		{
			ASSERT(!m_pending_device || m_pending_device == device);
			m_pending_device = device;
			return false;
		}

		m_executing_device = device;
		return true;
	}

	void PS2Controller::unlock_command(PS2Device* device)
	{
		ASSERT(m_executing_device == device);
		m_executing_device = nullptr;
		if (m_pending_device)
		{
			m_executing_device = m_pending_device;
			m_pending_device = nullptr;
			m_executing_device->update_command();
		}
	}

	BAN::ErrorOr<void> PS2Controller::device_send_byte(uint8_t device_index, uint8_t byte)
	{
		LockGuard _(m_lock);
		if (device_index == 1)
			TRY(send_byte(PS2::IOPort::COMMAND, PS2::Command::WRITE_TO_SECOND_PORT));
		TRY(send_byte(PS2::IOPort::DATA, byte));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::device_send_byte(PS2Device* device, uint8_t byte)
	{
		ASSERT(device);
		ASSERT(device == m_devices[0].ptr() || device == m_devices[1].ptr());
		TRY(device_send_byte(device == m_devices[0].ptr() ? 0 : 1, byte));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::device_read_ack(uint8_t device_index)
	{
		LockGuard _(m_lock);
		if (TRY(read_byte()) != PS2::Response::ACK)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 device on port {} did not respond with expected ACK", device_index);
			return BAN::Error::from_errno(EBADMSG);
		}
		return {};
	}

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
		TRY(send_command(PS2::Command::DISABLE_FIRST_PORT));
		TRY(send_command(PS2::Command::DISABLE_SECOND_PORT));

		// Step 4: Flush The Output Buffer
		while (!read_byte().is_error())
			continue;
		
		// Step 5: Set the Controller Configuration Byte
		TRY(send_command(PS2::Command::READ_CONFIG));
		uint8_t config = TRY(read_byte());
		config &= ~PS2::Config::INTERRUPT_FIRST_PORT;
		config &= ~PS2::Config::INTERRUPT_SECOND_PORT;
		config &= ~PS2::Config::TRANSLATION_FIRST_PORT;
		TRY(send_command(PS2::Command::WRITE_CONFIG, config));

		// Step 6: Perform Controller Self Test
		TRY(send_command(PS2::Command::TEST_CONTROLLER));
		if (TRY(read_byte()) != PS2::Response::TEST_CONTROLLER_PASS)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 Controller test failed");
			return BAN::Error::from_errno(ENODEV);
		}
		// NOTE: self test might reset the device so we set the config byte again
		TRY(send_command(PS2::Command::WRITE_CONFIG, config));

		// Step 7: Determine If There Are 2 Channels
		bool valid_ports[2] { true, false };
		if (config & PS2::Config::CLOCK_SECOND_PORT)
		{
			TRY(send_command(PS2::Command::ENABLE_SECOND_PORT));
			TRY(send_command(PS2::Command::READ_CONFIG));
			if (!(TRY(read_byte()) & PS2::Config::CLOCK_SECOND_PORT))
			{
				valid_ports[1] = true;
				TRY(send_command(PS2::Command::DISABLE_SECOND_PORT));
			}
		}

		// Step 8: Perform Interface Tests
		TRY(send_command(PS2::Command::TEST_FIRST_PORT));
		if (TRY(read_byte()) != PS2::Response::TEST_FIRST_PORT_PASS)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 first port test failed");
			valid_ports[0] = false;
		}
		if (valid_ports[1])
		{
			TRY(send_command(PS2::Command::TEST_SECOND_PORT));
			if (TRY(read_byte()) != PS2::Response::TEST_SECOND_PORT_PASS)
			{
				dwarnln_if(DEBUG_PS2, "PS/2 second port test failed");
				valid_ports[1] = false;
			}
		}
		if (!valid_ports[0] && !valid_ports[1])
			return {};
		
		// Step 9: Enable Devices (and disable scanning)
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			TRY(send_command(device == 0 ? PS2::Command::ENABLE_FIRST_PORT : PS2::Command::ENABLE_SECOND_PORT));
			if (set_scanning(device, false).is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 could not disable device scanning");
				valid_ports[device] = false;
			}
			TRY(send_command(device == 0 ? PS2::Command::DISABLE_FIRST_PORT : PS2::Command::DISABLE_SECOND_PORT));
		}

		// Step 10: Reset Devices
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			if (auto ret = reset_device(device); ret.is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device reset failed: {}", ret.error());
				valid_ports[device] = false;
				continue;
			}
			if (auto ret = set_scanning(device, false); ret.is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device scan disabling failed: {}", ret.error());
				valid_ports[device] = false;
				continue;
			}
		}

		// Step 11: Initialize Device Drivers
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			if (auto res = initialize_device(device); res.is_error())
				dwarnln_if(DEBUG_PS2, "PS/2 device initialization failed: {}", res.error());
		}

		if (m_devices[0])
		{
			m_devices[0]->set_irq(PS2::IRQ::DEVICE0);
			m_devices[0]->enable_interrupt();
			config |= PS2::Config::INTERRUPT_FIRST_PORT;
		}
		if (m_devices[1])
		{
			m_devices[1]->set_irq(PS2::IRQ::DEVICE1);
			m_devices[1]->enable_interrupt();
			config |= PS2::Config::INTERRUPT_SECOND_PORT;
		}

		TRY(send_command(PS2::Command::WRITE_CONFIG, config));

		for (uint8_t device = 0; device < 2; device++)
		{
			if (!m_devices[device])
				continue;
			m_devices[device]->send_initialize();
			DevFileSystem::get().add_device(m_devices[device]);
		}

		return {};
	}

	BAN::ErrorOr<void> PS2Controller::initialize_device(uint8_t device)
	{
		TRY(device_send_byte(device, PS2::DeviceCommand::IDENTIFY));
		TRY(device_read_ack(device));

		uint8_t bytes[2] {};
		uint8_t index = 0;
		for (uint8_t i = 0; i < 2; i++)
		{
			auto res = read_byte();
			if (res.is_error())
				break;
			bytes[index++] = res.value();
		}

		// Standard PS/2 Mouse
		if (index == 1 && (bytes[0] == 0x00))
		{
			dprintln_if(DEBUG_PS2, "PS/2 found mouse");
			m_devices[device] = TRY(PS2Mouse::create(*this));
		}
		// MF2 Keyboard
		else if (index == 2 && (bytes[0] == 0xAB && bytes[1] == 0x83))
		{
			dprintln_if(DEBUG_PS2, "PS/2 found keyboard");
			m_devices[device] = TRY(PS2Keyboard::create(*this));
		}

		if (m_devices[device])
			return {};

		dprintln_if(DEBUG_PS2, "PS/2 unsupported device {2H} {2H} ({} bytes) on port {}", bytes[0], bytes[1], index, device);
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<void> PS2Controller::reset_device(uint8_t device)
	{
		TRY(device_send_byte(device, PS2::DeviceCommand::RESET));
		TRY(device_read_ack(device));
		if (TRY(read_byte()) != PS2::Response::SELF_TEST_PASS)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 device self test failed");
			return BAN::Error::from_errno(ENODEV);
		}
		// device might send extra data
		while (!read_byte().is_error())
			continue;
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::set_scanning(uint8_t device, bool enabled)
	{
		TRY(device_send_byte(device, enabled ? PS2::DeviceCommand::ENABLE_SCANNING : PS2::DeviceCommand::DISABLE_SCANNING));
		TRY(device_read_ack(device));
		return {};
	}

}
