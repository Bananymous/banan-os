#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Controller.h>
#include <kernel/Input/PS2/Keyboard.h>
#include <kernel/Input/PS2/Mouse.h>
#include <kernel/IO.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::Input
{

	static constexpr uint64_t s_ps2_timeout_ms = 1000;

	static PS2Controller* s_instance = nullptr;

	BAN::ErrorOr<void> PS2Controller::send_byte(uint16_t port, uint8_t byte)
	{
		LockGuard _(m_mutex);
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
		LockGuard _(m_mutex);
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
		LockGuard _(m_mutex);
		TRY(send_byte(PS2::IOPort::COMMAND, command));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::send_command(PS2::Command command, uint8_t data)
	{
		LockGuard _(m_mutex);
		TRY(send_byte(PS2::IOPort::COMMAND, command));
		TRY(send_byte(PS2::IOPort::DATA, data));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::device_send_byte(uint8_t device_index, uint8_t byte)
	{
		LockGuard _(m_mutex);
		if (device_index == 1)
			TRY(send_byte(PS2::IOPort::COMMAND, PS2::Command::WRITE_TO_SECOND_PORT));
		TRY(send_byte(PS2::IOPort::DATA, byte));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::device_send_byte_and_wait_ack(uint8_t device_index, uint8_t byte)
	{
		LockGuard _(m_mutex);
		for (size_t attempt = 0; attempt < 10; attempt++)
		{
			TRY(device_send_byte(device_index, byte));
			uint8_t response = TRY(read_byte());
			if (response == PS2::Response::RESEND)
				continue;
			if (response == PS2::Response::ACK)
				return {};
			dwarnln_if(DEBUG_PS2, "PS/2 device on port {} did not respond with expected ACK, got {2H}", device_index, byte);
			return BAN::Error::from_errno(EBADMSG);
		}
		dwarnln_if(DEBUG_PS2, "PS/2 device on port {} is in resend loop", device_index, byte);
		return BAN::Error::from_errno(EBADMSG);
	}

	uint8_t PS2Controller::get_device_index(PS2Device* device) const
	{
		ASSERT(device);
		if (m_devices[0].ptr() == device)
			return 0;
		if (m_devices[1].ptr() == device)
			return 1;
		ASSERT_NOT_REACHED();
	}

	bool PS2Controller::append_command_queue(PS2Device* device, uint8_t command, uint8_t response_size)
	{
		SpinLockGuard _(m_command_lock);
		if (m_command_queue.size() + 1 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(Command {
			.state			= Command::State::NotSent,
			.device_index	= get_device_index(device),
			.out_data		= { command, 0x00 },
			.out_count		= 1,
			.in_count		= response_size,
			.send_index		= 0
		});
		return true;
	}

	bool PS2Controller::append_command_queue(PS2Device* device, uint8_t command, uint8_t data, uint8_t response_size)
	{
		SpinLockGuard _(m_command_lock);
		if (m_command_queue.size() + 1 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(Command {
			.state			= Command::State::NotSent,
			.device_index	= get_device_index(device),
			.out_data		= { command, data },
			.out_count		= 2,
			.in_count		= response_size,
			.send_index		= 0
		});
		return true;
	}

	void PS2Controller::update_command_queue()
	{
		Command command_copy;

		{
			SpinLockGuard _(m_command_lock);

			if (m_command_queue.empty())
				return;
			auto& command = m_command_queue.front();
			if (command.state == Command::State::WaitingResponse || command.state == Command::State::WaitingAck)
			{
				if (SystemTimer::get().ms_since_boot() >= m_command_send_time + s_ps2_timeout_ms)
				{
					dwarnln_if(DEBUG_PS2, "Command timedout");
					m_devices[command.device_index]->command_timedout(command.out_data, command.out_count);
					m_command_queue.pop();
				}
				return;
			}
			ASSERT(command.send_index < command.out_count);
			command.state = Command::State::WaitingAck;

			command_copy = command;
		}

		m_command_send_time = SystemTimer::get().ms_since_boot();
		if (auto ret = device_send_byte(command_copy.device_index, command_copy.out_data[command_copy.send_index]); ret.is_error())
			dwarnln_if(DEBUG_PS2, "PS/2 send command byte: {}", ret.error());
	}

	bool PS2Controller::handle_command_byte(PS2Device* device, uint8_t byte)
	{
		SpinLockGuard _(m_command_lock);

		if (m_command_queue.empty())
			return false;
		auto& command = m_command_queue.front();

		if (command.device_index != get_device_index(device))
			return false;

		switch (command.state)
		{
			case Command::State::NotSent:
			{
				return false;
			}
			case Command::State::Sending:
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device sent byte while middle of command send");
				return false;
			}
			case Command::State::WaitingResponse:
			{
				if (--command.in_count <= 0)
					m_command_queue.pop();
				return false;
			}
			case Command::State::WaitingAck:
			{
				switch (byte)
				{
					case PS2::Response::ACK:
					{
						if (++command.send_index < command.out_count)
							command.state = Command::State::Sending;
						else if (command.in_count > 0)
							command.state = Command::State::WaitingResponse;
						else
							m_command_queue.pop();
						return true;
					}
					case PS2::Response::RESEND:
						command.state = Command::State::Sending;
						return true;
					default:
						dwarnln_if(DEBUG_PS2, "PS/2 expected ACK got {2H}", byte);
						command.state = Command::State::Sending;
						return true;
				}
				break;
			}
		}
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> PS2Controller::initialize(uint8_t scancode_set)
	{
		ASSERT(s_instance == nullptr);
		s_instance = new PS2Controller;
		if (s_instance == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([] { delete s_instance; });
		TRY(s_instance->initialize_impl(scancode_set));
		guard.disable();
		return {};
	}

	PS2Controller& PS2Controller::get()
	{
		ASSERT(s_instance != nullptr);
		return *s_instance;
	}

	struct PS2DeviceInitInfo
	{
		PS2Controller* controller;
		bool valid_ports[2];
		uint8_t scancode_set;
		uint8_t config;
		BAN::Atomic<bool> thread_started;
	};

	BAN::ErrorOr<void> PS2Controller::initialize_impl(uint8_t scancode_set)
	{
		constexpr size_t iapc_flag_off = offsetof(ACPI::FADT, iapc_boot_arch);
		constexpr size_t iapc_flag_end = iapc_flag_off + sizeof(ACPI::FADT::iapc_boot_arch);

		// If user provided scan code set, skip FADT detection
		if (scancode_set == 0xFF)
			scancode_set = 0;
		else if (scancode_set == 0)
		{
			// Determine if the PS/2 Controller Exists
			auto* fadt = static_cast<const ACPI::FADT*>(ACPI::ACPI::get().get_header("FACP"_sv, 0));
			if (fadt && fadt->revision >= 3 && fadt->length >= iapc_flag_end && !(fadt->iapc_boot_arch & (1 << 1)))
			{
				dwarnln_if(DEBUG_PS2, "No PS/2 available");
				return BAN::Error::from_errno(ENODEV);
			}
		}

		// Disable Devices
		TRY(send_command(PS2::Command::DISABLE_FIRST_PORT));
		TRY(send_command(PS2::Command::DISABLE_SECOND_PORT));

		// Flush The Output Buffer
		while (!read_byte().is_error())
			continue;

		// Set the Controller Configuration Byte
		TRY(send_command(PS2::Command::READ_CONFIG));
		uint8_t config = TRY(read_byte());
		config &= ~PS2::Config::INTERRUPT_FIRST_PORT;
		config &= ~PS2::Config::INTERRUPT_SECOND_PORT;
		config &= ~PS2::Config::TRANSLATION_FIRST_PORT;
		TRY(send_command(PS2::Command::WRITE_CONFIG, config));

		// Perform Controller Self Test
		TRY(send_command(PS2::Command::TEST_CONTROLLER));
		if (TRY(read_byte()) != PS2::Response::TEST_CONTROLLER_PASS)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 Controller test failed");
			return BAN::Error::from_errno(ENODEV);
		}
		// NOTE: self test might reset the device so we set the config byte again
		TRY(send_command(PS2::Command::WRITE_CONFIG, config));

		// Determine If There Are 2 Channels
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

		// Perform Interface Tests
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

		// Reserve IRQs
		if (valid_ports[0] && InterruptController::get().reserve_irq(PS2::IRQ::DEVICE0).is_error())
		{
			dwarnln("Could not reserve irq for PS/2 port 1");
			valid_ports[0] = false;
		}
		if (valid_ports[1] && InterruptController::get().reserve_irq(PS2::IRQ::DEVICE1).is_error())
		{
			dwarnln("Could not reserve irq for PS/2 port 2");
			valid_ports[1] = false;
		}

		PS2DeviceInitInfo info {
			.controller = this,
			.valid_ports = { valid_ports[0], valid_ports[1] },
			.scancode_set = scancode_set,
			.config = config,
			.thread_started { false },
		};

		auto* init_thread = TRY(Thread::create_kernel(
			[](void* info) {
				static_cast<PS2DeviceInitInfo*>(info)->controller->device_initialize_task(info);
			}, &info
		));
		TRY(Processor::scheduler().add_thread(init_thread));

		while (!info.thread_started)
			Processor::pause();

		return {};
	}

	void PS2Controller::device_initialize_task(void* _info)
	{
		bool valid_ports[2];
		uint8_t scancode_set;
		uint8_t config;

		{
			auto& info = *static_cast<PS2DeviceInitInfo*>(_info);
			valid_ports[0] = info.valid_ports[0];
			valid_ports[1] = info.valid_ports[1];
			scancode_set = info.scancode_set;
			config = info.config;
			info.thread_started = true;
		}

		// Initialize devices
		for (uint8_t device = 0; device < 2; device++)
		{
			if (!valid_ports[device])
				continue;
			if (auto ret = send_command(device == 0 ? PS2::Command::ENABLE_FIRST_PORT : PS2::Command::ENABLE_SECOND_PORT); ret.is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device enable failed: {}", ret.error());
				continue;
			}
			if (auto res = identify_device(device, scancode_set); res.is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device initialization failed: {}", res.error());
				(void)send_command(device == 0 ? PS2::Command::DISABLE_FIRST_PORT : PS2::Command::DISABLE_SECOND_PORT);
				continue;
			}
		}

		if (!m_devices[0] && !m_devices[1])
			return;

		// Enable irqs on valid devices
		if (m_devices[0])
		{
			m_devices[0]->set_irq(PS2::IRQ::DEVICE0);
			InterruptController::get().enable_irq(PS2::IRQ::DEVICE0);
			config |= PS2::Config::INTERRUPT_FIRST_PORT;
		}
		if (m_devices[1])
		{
			m_devices[1]->set_irq(PS2::IRQ::DEVICE1);
			InterruptController::get().enable_irq(PS2::IRQ::DEVICE1);
			config |= PS2::Config::INTERRUPT_SECOND_PORT;
		}

		if (auto ret = send_command(PS2::Command::WRITE_CONFIG, config); ret.is_error())
		{
			dwarnln("PS2 failed to enable interrupts: {}", ret.error());
			m_devices[0].clear();
			m_devices[1].clear();
			return;
		}

		// Send device initialization sequence after interrupts are enabled
		for (uint8_t i = 0; i < 2; i++)
			if (m_devices[i])
				m_devices[i]->send_initialize();
	}

	BAN::ErrorOr<void> PS2Controller::identify_device(uint8_t device, uint8_t scancode_set)
	{
		// Reset device
		TRY(device_send_byte_and_wait_ack(device, PS2::DeviceCommand::RESET));
		if (TRY(read_byte()) != PS2::Response::SELF_TEST_PASS)
		{
			dwarnln_if(DEBUG_PS2, "PS/2 device self test failed");
			return BAN::Error::from_errno(ENODEV);
		}
		while (!read_byte().is_error())
			continue;

		// Disable scanning and flush buffer
		TRY(device_send_byte_and_wait_ack(device, PS2::DeviceCommand::DISABLE_SCANNING));
		while (!read_byte().is_error())
			continue;

		// Identify device
		TRY(device_send_byte_and_wait_ack(device, PS2::DeviceCommand::IDENTIFY));

		// Read up to 2 identification bytes
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
			return {};
		}

		// MF2 Keyboard
		if (index == 2 && bytes[0] == 0xAB)
		{
			switch (bytes[1])
			{
				case 0x41: // MF2 Keyboard (translated but my laptop uses this :))
				case 0x83: // MF2 Keyboard
				case 0xC1: // MF2 Keyboard
				case 0x84: // Thinkpad KB
					dprintln_if(DEBUG_PS2, "PS/2 found keyboard");
					m_devices[device] = TRY(PS2Keyboard::create(*this, scancode_set));
					return {};
				default:
					break;
			}
		}

		dprintln_if(DEBUG_PS2, "PS/2 unsupported device {2H} {2H} ({} bytes) on port {}", bytes[0], bytes[1], index, device);
		return BAN::Error::from_errno(ENOTSUP);
	}

}
