#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/Resource.h>
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
			if (IO::inb(m_command_port) & PS2::Status::INPUT_STATUS)
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
			if (!(IO::inb(m_command_port) & PS2::Status::OUTPUT_STATUS))
				continue;
			return IO::inb(m_data_port);
		}
		return BAN::Error::from_errno(ETIMEDOUT);
	}

	BAN::ErrorOr<void> PS2Controller::send_command(PS2::Command command)
	{
		LockGuard _(m_mutex);
		TRY(send_byte(m_command_port, command));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::send_command(PS2::Command command, uint8_t data)
	{
		LockGuard _(m_mutex);
		TRY(send_byte(m_command_port, command));
		TRY(send_byte(m_data_port, data));
		return {};
	}

	BAN::ErrorOr<void> PS2Controller::device_send_byte(uint8_t device_index, uint8_t byte)
	{
		LockGuard _(m_mutex);
		if (device_index == 1)
			TRY(send_byte(m_command_port, PS2::Command::WRITE_TO_SECOND_PORT));
		TRY(send_byte(m_data_port, byte));
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
		struct DeviceInfo
		{
			PS2Controller::DeviceType type;
			uint8_t interrupt;
		};
		DeviceInfo devices[2];
		uint8_t scancode_set;
		uint8_t config;
		BAN::Atomic<bool> thread_started;
	};

	static bool has_legacy_8042()
	{
		constexpr size_t iapc_flag_off = offsetof(ACPI::FADT, iapc_boot_arch);
		constexpr size_t iapc_flag_end = iapc_flag_off + sizeof(ACPI::FADT::iapc_boot_arch);

		const auto* fadt = static_cast<const ACPI::FADT*>(ACPI::ACPI::get().get_header("FACP"_sv, 0));
		if (!fadt || fadt->revision < 3 || fadt->length < iapc_flag_end)
			return true;

		if (!(fadt->iapc_boot_arch & (1 << 1)))
			return false;

		return true;
	}

	BAN::ErrorOr<void> PS2Controller::initialize_impl(uint8_t scancode_set)
	{
		PS2DeviceInitInfo::DeviceInfo devices[2] {
			PS2DeviceInitInfo::DeviceInfo {
				.type = DeviceType::None,
				.interrupt = 0xFF,
			},
			PS2DeviceInitInfo::DeviceInfo {
				.type = DeviceType::None,
				.interrupt = 0xFF,
			},
		};

		if (auto* ns = ACPI::ACPI::get().acpi_namespace())
		{
			dprintln("Looking for PS/2 devices in ACPI namespace...");

			const auto lookup_devices =
				[ns](const BAN::Vector<BAN::String>& eisa_id_strs) -> decltype(ns->find_device_with_eisa_id(""_sv))
				{
					BAN::Vector<BAN::StringView> eisa_ids;
					TRY(eisa_ids.reserve(eisa_id_strs.size()));
					for (const auto& str : eisa_id_strs)
						TRY(eisa_ids.push_back(str.sv()));
					return ns->find_device_with_eisa_id(eisa_ids.span());
				};

			struct PS2ResourceSetting
			{
				DeviceType type;
				BAN::Optional<uint16_t> command_port;
				BAN::Optional<uint16_t> data_port;
				BAN::Optional<uint8_t> irq;
			};

			const auto get_device_info =
				[ns](const ACPI::AML::Scope& scope, const auto& type) -> BAN::ErrorOr<PS2ResourceSetting>
				{
					auto [sta_path, sta_obj] = TRY(ns->find_named_object(scope, TRY(ACPI::AML::NameString::from_string("_STA"_sv)), true));
					if (sta_obj == nullptr)
						return BAN::Error::from_errno(ENODEV);
					const auto sta_result = TRY(ACPI::AML::convert_node(TRY(ACPI::AML::evaluate_node(sta_path, sta_obj->node)), ACPI::AML::ConvInteger, -1)).as.integer.value;
					if ((sta_result & 0b11) != 0b11)
						return BAN::Error::from_errno(ENODEV);

					auto [crs_path, crs_obj] = TRY(ns->find_named_object(scope, TRY(ACPI::AML::NameString::from_string("_CRS"_sv)), true));
					if (crs_obj == nullptr)
						return PS2ResourceSetting {};

					PS2ResourceSetting result;
					result.type = type;

					BAN::Optional<ACPI::ResourceData> data;
					ACPI::ResourceParser parser({ crs_obj->node.as.str_buf->bytes, crs_obj->node.as.str_buf->size });
					while ((data = parser.get_next()).has_value())
					{
						switch (data->type)
						{
							case ACPI::ResourceData::Type::IOPort:
								if (data->as.io_port.range_min_base != data->as.io_port.range_max_base)
									break;
								if (data->as.io_port.range_length != 1)
									break;
								if (!result.data_port.has_value())
									result.data_port = data->as.io_port.range_min_base;
								else if (!result.command_port.has_value())
									result.command_port = data->as.io_port.range_min_base;
								break;
							case ACPI::ResourceData::Type::FixedIOPort:
								if (data->as.fixed_io_port.range_length != 1)
									break;
								if (!result.data_port.has_value())
									result.data_port = data->as.fixed_io_port.range_base;
								else if (!result.command_port.has_value())
									result.command_port = data->as.fixed_io_port.range_base;
								break;
							case ACPI::ResourceData::Type::IRQ:
								if (__builtin_popcount(data->as.irq.irq_mask) != 1)
									break;
								for (int i = 0; i < 16; i++)
									if (data->as.irq.irq_mask & (1 << i))
										result.irq = i;
								break;
							default:
								break;
						}
					}

					return result;
				};

			BAN::Vector<PS2ResourceSetting> acpi_devices;

			{
				BAN::Vector<BAN::String> kbd_eisa_ids;
				TRY(kbd_eisa_ids.reserve(0x1F));
				for (uint8_t i = 0; i <= 0x0B; i++)
					TRY(kbd_eisa_ids.push_back(TRY(BAN::String::formatted("PNP03{2H}", i))));

				auto kbds = TRY(lookup_devices(kbd_eisa_ids));
				for (auto& kbd : kbds)
					if (auto ret = get_device_info(kbd, DeviceType::Keyboard); !ret.is_error())
						TRY(acpi_devices.push_back(ret.release_value()));
			}

			{
				BAN::Vector<BAN::String> mouse_eisa_ids;
				TRY(mouse_eisa_ids.reserve(0x1F));
				for (uint8_t i = 0; i <= 0x23; i++)
					TRY(mouse_eisa_ids.push_back(TRY(BAN::String::formatted("PNP0F{2H}", i))));

				auto mice = TRY(lookup_devices(mouse_eisa_ids));
				for (auto& mouse : mice)
					if (auto ret = get_device_info(mouse, DeviceType::Mouse); !ret.is_error())
						TRY(acpi_devices.push_back(ret.release_value()));
			}

			dprintln("Found {} PS/2 devices from ACPI namespace", acpi_devices.size());
			if (acpi_devices.empty())
				return {};

			if (acpi_devices.size() > 2)
			{
				dwarnln("TODO: over 2 PS/2 devices");
				while (acpi_devices.size() > 2)
					acpi_devices.pop_back();
			}

			if (acpi_devices.size() == 2)
			{
				const auto compare_optionals =
					[](const auto& a, const auto& b) -> bool
					{
						if (!a.has_value() || !b.has_value())
							return true;
						if (a.value() != b.value())
							return false;
						return true;
					};

				const bool can_use_both =
					compare_optionals(acpi_devices[0].command_port, acpi_devices[1].command_port) &&
					compare_optionals(acpi_devices[0].data_port,    acpi_devices[1].data_port);

				if (!can_use_both)
				{
					dwarnln("TODO: multiple PS/2 controllers");
					acpi_devices.pop_back();
				}
			}

			BAN::Optional<uint16_t> command_port;
			command_port = acpi_devices[0].command_port;
			if (!command_port.has_value() && acpi_devices.size() >= 2)
				command_port = acpi_devices[1].command_port;
			if (command_port.has_value())
				m_command_port = command_port.value();

			BAN::Optional<uint16_t> data_port;
			data_port = acpi_devices[0].data_port;
			if (!data_port.has_value() && acpi_devices.size() >= 2)
				data_port = acpi_devices[1].data_port;
			if (data_port.has_value())
				m_data_port = data_port.value();

			devices[0] = {
				.type      = acpi_devices[0].type,
				.interrupt = acpi_devices[0].irq.value_or(PS2::INTERRUPT_FIRST_PORT)
			};

			if (acpi_devices.size() > 1)
			{
				devices[1] = {
					.type      = acpi_devices[1].type,
					.interrupt = acpi_devices[1].irq.value_or(PS2::INTERRUPT_SECOND_PORT)
				};
			}
		}
		else if (has_legacy_8042())
		{
			devices[0] = {
				.type = DeviceType::Unknown,
				.interrupt = PS2::INTERRUPT_FIRST_PORT,
			};
			devices[1] = {
				.type = DeviceType::Unknown,
				.interrupt = PS2::INTERRUPT_SECOND_PORT,
			};
		}
		else
		{
			dwarnln("No PS/2 controller found");
			return {};
		}

		// Disable Devices
		TRY(send_command(PS2::Command::DISABLE_FIRST_PORT));
		TRY(send_command(PS2::Command::DISABLE_SECOND_PORT));

		// Flush The Output Buffer
		while (IO::inb(m_command_port) & PS2::Status::OUTPUT_STATUS)
			IO::inb(m_data_port);

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
		if ((config & PS2::Config::CLOCK_SECOND_PORT) && devices[1].type != DeviceType::None)
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
		if (valid_ports[0] && InterruptController::get().reserve_irq(devices[0].interrupt).is_error())
		{
			dwarnln("Could not reserve irq for PS/2 port 1");
			valid_ports[0] = false;
		}
		if (valid_ports[1] && InterruptController::get().reserve_irq(devices[1].interrupt).is_error())
		{
			dwarnln("Could not reserve irq for PS/2 port 2");
			valid_ports[1] = false;
		}

		if (!valid_ports[0])
			devices[0].type = DeviceType::None;
		if (!valid_ports[1])
			devices[1].type = DeviceType::None;

		PS2DeviceInitInfo info {
			.controller = this,
			.devices = { devices[0], devices[1] },
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
		PS2DeviceInitInfo::DeviceInfo devices[2];
		uint8_t scancode_set;
		uint8_t config;

		{
			auto& info = *static_cast<PS2DeviceInitInfo*>(_info);
			devices[0] = info.devices[0];
			devices[1] = info.devices[1];
			scancode_set = info.scancode_set;
			config = info.config;
			info.thread_started = true;
		}

		// Initialize devices
		for (uint8_t i = 0; i < 2; i++)
		{
			if (devices[i].type == DeviceType::None)
				continue;
			if (auto ret = send_command(i == 0 ? PS2::Command::ENABLE_FIRST_PORT : PS2::Command::ENABLE_SECOND_PORT); ret.is_error())
			{
				dwarnln_if(DEBUG_PS2, "PS/2 device enable failed: {}", ret.error());
				continue;
			}

			if (devices[i].type == DeviceType::Unknown)
			{
				if (auto res = identify_device(i); !res.is_error())
					devices[i].type = res.value();
				else
				{
					dwarnln_if(DEBUG_PS2, "PS/2 device initialization failed: {}", res.error());
					(void)send_command(i == 0 ? PS2::Command::DISABLE_FIRST_PORT : PS2::Command::DISABLE_SECOND_PORT);
					continue;
				}
			}

			switch (devices[i].type)
			{
				case DeviceType::Unknown:
					break;
				case DeviceType::None:
					ASSERT_NOT_REACHED();
				case DeviceType::Keyboard:
					dprintln_if(DEBUG_PS2, "PS/2 found keyboard");
					if (auto ret = PS2Keyboard::create(*this, scancode_set); !ret.is_error())
						m_devices[i] = ret.release_value();
					else
						dwarnln_if(DEBUG_PS2, "... {}", ret.error());
					break;
				case DeviceType::Mouse:
					dprintln_if(DEBUG_PS2, "PS/2 found mouse");
					if (auto ret = PS2Mouse::create(*this); !ret.is_error())
						m_devices[i] = ret.release_value();
					else
						dwarnln_if(DEBUG_PS2, "... {}", ret.error());
					break;
			}
		}

		if (!m_devices[0] && !m_devices[1])
			return;

		// Enable irqs on valid devices
		if (m_devices[0])
		{
			m_devices[0]->set_irq(devices[0].interrupt);
			InterruptController::get().enable_irq(devices[0].interrupt);
			config |= PS2::Config::INTERRUPT_FIRST_PORT;
		}
		if (m_devices[1])
		{
			m_devices[1]->set_irq(devices[1].interrupt);
			InterruptController::get().enable_irq(devices[1].interrupt);
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

	BAN::ErrorOr<PS2Controller::DeviceType> PS2Controller::identify_device(uint8_t device)
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
			return DeviceType::Mouse;

		// MF2 Keyboard
		if (index == 2 && bytes[0] == 0xAB)
		{
			switch (bytes[1])
			{
				case 0x41: // MF2 Keyboard (translated but my laptop uses this :))
				case 0x83: // MF2 Keyboard
				case 0xC1: // MF2 Keyboard
				case 0x84: // Thinkpad KB
					return DeviceType::Keyboard;
				default:
					break;
			}
		}

		dprintln_if(DEBUG_PS2, "PS/2 unsupported device {2H} {2H} ({} bytes) on port {}", bytes[0], bytes[1], index, device);
		return DeviceType::Unknown;
	}

}
