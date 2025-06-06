#include <BAN/ScopeGuard.h>
#include <kernel/USB/Hub/Definitions.h>
#include <kernel/USB/Hub/HubDriver.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	USBHubDriver::USBHubDriver(USBDevice& device, const USBDevice::DeviceDescriptor& desciptor)
		: m_device(device)
		, m_descriptor(desciptor)
	{}

	USBHubDriver::~USBHubDriver()
	{
		m_running = false;
		m_changed_port_blocker.unblock();
		while (m_port_updater)
			continue;

		for (const auto info : m_ports)
		{
			if (info.slot == 0)
				continue;
			m_device.deinitialize_device_slot(info.slot);
		}
	}

	BAN::ErrorOr<void> USBHubDriver::initialize()
	{
#if DEBUG_USB_HUB
		switch (m_device.speed_class())
		{
			case USB::SpeedClass::LowSpeed:
				dprintln("configuring low speed hub");
				break;
			case USB::SpeedClass::FullSpeed:
				dprintln("configuring full speed hub");
				break;
			case USB::SpeedClass::HighSpeed:
				dprintln("configuring high speed hub");
				break;
			case USB::SpeedClass::SuperSpeed:
				dprintln("configuring super speed hub");
				break;
		}
	#endif

		m_data_region = TRY(DMARegion::create(PAGE_SIZE));

		if (m_descriptor.configurations.empty())
		{
			dwarnln("USB hub does not have any configurations");
			return BAN::Error::from_errno(EFAULT);
		}
		const auto& configuration = m_descriptor.configurations[0];

		{
			dprintln_if(DEBUG_USB_HUB, "setting configuration");
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Standard | USB::RequestType::Device;
			request.bRequest = USB::SET_CONFIGURATION;
			request.wValue = configuration.desciptor.bConfigurationValue;
			request.wIndex = 0;
			request.wLength = 0;
			TRY(m_device.send_request(request, 0));
			dprintln_if(DEBUG_USB_HUB, "  -> done");
		}

		if (configuration.interfaces.empty())
		{
			dwarnln("USB hub does not have any interfaces");
			return BAN::Error::from_errno(EFAULT);
		}
		const auto* interface = &configuration.interfaces[0];

		// High speed hubs may have alternate multi tt interface
		if (m_device.speed_class() == USB::SpeedClass::HighSpeed)
		{
			for (size_t i = 0; i < configuration.interfaces.size(); i++)
			{
				if (configuration.interfaces[i].descriptor.bInterfaceProtocol != 2)
					continue;
				interface = &configuration.interfaces[i];
				break;
			}

			if (interface->descriptor.bAlternateSetting != 0)
			{
				dprintln_if(DEBUG_USB_HUB, "enabling multi tt interface");
				USBDeviceRequest request;
				request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Standard | USB::RequestType::Interface;
				request.bRequest = USB::SET_INTERFACE;
				request.wValue = interface->descriptor.bAlternateSetting;
				request.wIndex = interface->descriptor.iInterface;
				request.wLength = 0;
				TRY(m_device.send_request(request, 0));
				dprintln_if(DEBUG_USB_HUB, "  -> done");
			}

			m_is_multi_tt = (interface->descriptor.bInterfaceProtocol == 2);
		}

		if (interface->endpoints.size() != 1)
		{
			dwarnln("USB hub has {} endpoints", interface->endpoints.size());
			return BAN::Error::from_errno(EFAULT);
		}
		const auto& endpoint = interface->endpoints[0];

		m_is_usb3 = (m_device.speed_class() == USB::SpeedClass::SuperSpeed);
		m_is_usb2 = !m_is_usb3;
		ASSERT(m_is_usb2 ^ m_is_usb3);

		// Set multi tt to false until we know tt think time
		dprintln_if(DEBUG_USB_HUB, "configuring endpoint with 4 ports");
		TRY(m_device.configure_endpoint(endpoint.descriptor, {
			.number_of_ports = m_port_count,
			.multi_tt = false,
			.tt_think_time = 0
		}));
		dprintln_if(DEBUG_USB_HUB, "  -> done");

		// USB 3 devices use route_string and hub depth for routing packets
		if (m_is_usb3)
		{
			dprintln_if(DEBUG_USB_HUB, "setting hub depth");
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Device;
			request.bRequest = 12; // SET_HUB_DEPTH
			request.wValue = m_device.depth();
			request.wIndex = 0;
			request.wLength = 0;
			TRY(m_device.send_request(request, 0));
			dprintln_if(DEBUG_USB_HUB, "  -> done");
		}

		uint8_t tt_think_time = 0;

		// Hub descriptor has to be requested after device is configured
		{
			dprintln_if(DEBUG_USB_HUB, "getting hub descriptor");
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Device;
			request.bRequest      = USB::Request::GET_DESCRIPTOR;
			request.wValue        = m_is_usb3 ? 0x2A00 : 0x2900;
			request.wIndex        = 0;
			request.wLength       = 8;
			if (TRY(m_device.send_request(request, m_data_region->paddr())) < 8)
			{
				dwarnln("USB hub did not respond with full hub descriptor");
				return BAN::Error::from_errno(EFAULT);
			}
			dprintln_if(DEBUG_USB_HUB, "  -> done");

			const auto& hub_descriptor = *reinterpret_cast<USBHub::HubDescriptor*>(m_data_region->vaddr());

			m_port_count = hub_descriptor.bNbrPorts;

			if (m_device.speed_class() == USB::SpeedClass::HighSpeed)
				tt_think_time = (hub_descriptor.wHubCharacteristics >> 5) & 0x03;
		}

		if (m_port_count > 31)
		{
			dwarnln("USB hubs only support up to 31 ports");
			m_port_count = 31;
		}

		TRY(m_ports.resize(m_port_count));

		dprintln_if(DEBUG_USB_HUB, "re-configuring endpoint with {} ports", m_port_count);
		TRY(m_device.configure_endpoint(endpoint.descriptor, {
			.number_of_ports = m_port_count,
			.multi_tt = m_is_multi_tt,
			.tt_think_time = tt_think_time
		}));
		dprintln_if(DEBUG_USB_HUB, "  -> done");

		m_endpoint_id = (endpoint.descriptor.bEndpointAddress & 0x0F) * 2 + !!(endpoint.descriptor.bEndpointAddress & 0x80);
		m_device.send_data_buffer(m_endpoint_id, m_data_region->paddr() + sizeof(USBHub::PortStatus), m_port_count / 8 + 1);

		// Reset all ports in powered off state
		for (size_t port_id = 1; port_id <= m_port_count; port_id++)
		{
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Other;
			request.bRequest = USB::Request::GET_STATUS;
			request.wValue = 0;
			request.wIndex = port_id;
			request.wLength = sizeof(USBHub::PortStatus);
			auto result = m_device.send_request(request, m_data_region->paddr());
			if (result.is_error() || result.value() != sizeof(USBHub::PortStatus))
			{
				dwarnln("Failed to get port {} status");
				continue;
			}

			const auto& port_status = *reinterpret_cast<volatile USBHub::PortStatus*>(m_data_region->vaddr());

			const bool is_powered =
				(m_is_usb3 && port_status.wPortStatus.usb3.power) ||
				(m_is_usb2 && port_status.wPortStatus.usb2.power);

			if (!is_powered)
				if (auto ret = set_port_feature(port_id, USBHub::PORT_POWER); ret.is_error())
					dwarnln("Failed to power on USB hub port {}: {}", port_id, ret.error());

			dprintln_if(DEBUG_USB_HUB, "port {}, status {4H}, changed {4H}",
				port_id,
				*(uint16_t*)(m_data_region->vaddr() + 0),
				*(uint16_t*)(m_data_region->vaddr() + 2)
			);

			m_changed_ports |= 1u << port_id;
		}

		m_port_updater = Process::create_kernel([](void* data) { reinterpret_cast<USBHubDriver*>(data)->port_updater_task(); }, this);
		if (m_port_updater == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		return {};
	}

	BAN::ErrorOr<void> USBHubDriver::clear_port_feature(uint8_t port, uint8_t feature)
	{
		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Other;
		request.bRequest = USB::Request::CLEAR_FEATURE;
		request.wValue = feature;
		request.wIndex = port;
		request.wLength = 0;
		TRY(m_device.send_request(request, 0));
		return {};
	}

	BAN::ErrorOr<void> USBHubDriver::set_port_feature(uint8_t port, uint8_t feature)
	{
		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Other;
		request.bRequest = USB::Request::SET_FEATURE;
		request.wValue = feature;
		request.wIndex = port;
		request.wLength = 0;
		TRY(m_device.send_request(request, 0));
		return {};
	}

	void USBHubDriver::port_updater_task()
	{
		m_device.register_hub_to_init();

		while (m_running && !m_device.can_start_hub_init())
			Processor::yield();

		uint64_t last_port_update_ms = SystemTimer::get().ms_since_boot();
		while (m_running)
		{
			uint8_t changed_port = 0xFF;

			{
				const auto temp = m_changed_ports.exchange(0);
				if (temp == 0)
				{
					// If there has been no changed ports in 100ms, this hub has initialized its devices
					if (!m_is_init_done && SystemTimer::get().ms_since_boot() - last_port_update_ms >= 100)
					{
						m_device.mark_hub_init_done();
						m_is_init_done = true;
					}

					// FIXME: race condition
					m_changed_port_blocker.block_with_timeout_ms(100, nullptr);
					continue;
				}

				last_port_update_ms = SystemTimer::get().ms_since_boot();

				for (size_t i = 0; i <= m_ports.size(); i++)
				{
					if (!(temp & (1u << i)))
						continue;
					changed_port = i;
					break;
				}

				m_changed_ports |= temp & ~(1u << changed_port);
			}

			if (changed_port == 0)
			{
				dprintln_if(DEBUG_USB_HUB, "TODO: USB Hub changed");
				continue;
			}

			{
				USBDeviceRequest request;
				request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Other;
				request.bRequest = USB::Request::GET_STATUS;
				request.wValue = 0;
				request.wIndex = changed_port;
				request.wLength = sizeof(USBHub::PortStatus);
				auto result = m_device.send_request(request, m_data_region->paddr());
				if (result.is_error() || result.value() != sizeof(USBHub::PortStatus))
				{
					dwarnln("Failed to get port {} status", changed_port);
					continue;
				}
			}

			const USBHub::PortStatus port_status {
				.wPortStatus  = { .raw = reinterpret_cast<volatile USBHub::PortStatus*>(m_data_region->vaddr())->wPortStatus.raw  },
				.wPortChanged = { .raw = reinterpret_cast<volatile USBHub::PortStatus*>(m_data_region->vaddr())->wPortChanged.raw },
			};

			if (port_status.wPortChanged.connection)
			{
				(void)clear_port_feature(changed_port, USBHub::C_PORT_CONNECTION);

				if (port_status.wPortStatus.connection)
				{
					// USB 2 devices have to be reset to enter enabled state
					if (auto ret = set_port_feature(changed_port, USBHub::PORT_RESET); ret.is_error())
						dwarnln("Failed to reset USB hub port {}: {}", changed_port, ret.error());
				}
				else
				{
					// Cleanup port on disconnection
					auto& port_info = m_ports[changed_port - 1];
					if (port_info.slot != 0)
						m_device.deinitialize_device_slot(port_info.slot);
					port_info = {};
				}
			}

			if (port_status.wPortChanged.reset)
			{
				// If this is USB3 device and the port is not in enabled state don't
				// clear reset changed so hub will trigger another port changed interrupt
				if (!port_status.wPortStatus.connection || !m_is_usb3 || port_status.wPortStatus.enable)
					(void)clear_port_feature(changed_port, USBHub::C_PORT_RESET);
			}

			if (port_status.wPortChanged.over_current)
			{
				(void)clear_port_feature(changed_port, USBHub::C_PORT_OVER_CURRENT);
				dwarnln_if(DEBUG_USB_HUB, "TODO: USB hub port over current change");
			}

			if (m_is_usb2)
			{
				if (port_status.wPortChanged.usb2.enable)
					(void)clear_port_feature(changed_port, USBHub::C_PORT_ENABLE);

				if (port_status.wPortChanged.usb2.suspend)
				{
					(void)clear_port_feature(changed_port, USBHub::C_PORT_SUSPEND);
					dwarnln_if(DEBUG_USB_HUB, "TODO: USB hub port suspend change");
				}
			}
			else if (m_is_usb3)
			{
				if (port_status.wPortChanged.usb3.link_state)
					(void)clear_port_feature(changed_port, USBHub::C_PORT_LINK_STATE);

				if (port_status.wPortChanged.usb3.bh_reset)
				{
					(void)clear_port_feature(changed_port, USBHub::C_BH_PORT_RESET);
					dwarnln_if(DEBUG_USB_HUB, "TODO: USB hub bh port reset change");
				}

				if (port_status.wPortChanged.usb3.config_error)
				{
					(void)clear_port_feature(changed_port, USBHub::C_PORT_CONFIG_ERROR);
					dwarnln_if(DEBUG_USB_HUB, "TODO: USB hub port config error chage");
				}
			}
			else
			{
				ASSERT_NOT_REACHED();
			}

			// Initialize new devices that have not failed initialization
			if (port_status.wPortStatus.enable && !m_ports[changed_port - 1].slot && !m_ports[changed_port - 1].failed)
			{
				USB::SpeedClass speed_class;
				if (m_is_usb3)
					speed_class = USB::SpeedClass::SuperSpeed;
				else if (port_status.wPortStatus.usb2.low_speed)
					speed_class = USB::SpeedClass::LowSpeed;
				else if (port_status.wPortStatus.usb2.high_speed)
					speed_class = USB::SpeedClass::HighSpeed;
				else
					speed_class = USB::SpeedClass::FullSpeed;

#if DEBUG_USB_HUB
				const char* speed_str = "<invalid>";
				switch (speed_class)
				{
					case USB::SpeedClass::LowSpeed:   speed_str = "low";   break;
					case USB::SpeedClass::FullSpeed:  speed_str = "full";  break;
					case USB::SpeedClass::HighSpeed:  speed_str = "high";  break;
					case USB::SpeedClass::SuperSpeed: speed_str = "super"; break;
				}
				dprintln("Initializing {} speed device on hub port {}", speed_str, changed_port);
#endif

				auto result = m_device.initialize_device_on_hub_port(changed_port, speed_class);
				if (!result.is_error())
					m_ports[changed_port - 1].slot = result.value();
				else
				{
					m_ports[changed_port - 1].failed = true;
					dwarnln("Failed to initialize USB hub port {}: {}", changed_port, result.error());
				}
			}
		}

		m_port_updater = nullptr;
	}

	void USBHubDriver::handle_stall(uint8_t endpoint_id)
	{
		(void)endpoint_id;
		dwarnln("TODO: USB hub handle stall");
	}

	void USBHubDriver::handle_input_data(size_t byte_count, uint8_t endpoint_id)
	{
		if (endpoint_id != m_endpoint_id)
			return;

		BAN::ScopeGuard query_changes([&] {
			m_device.send_data_buffer(m_endpoint_id, m_data_region->paddr() + sizeof(USBHub::PortStatus), m_port_count / 8 + 1);
		});

		if (m_ports.size() / 8 + 1 < byte_count)
			byte_count = m_ports.size() / 8 + 1;

		uint32_t new_ports = 0;
		const auto* bitmap = reinterpret_cast<const uint8_t*>(m_data_region->vaddr() + sizeof(USBHub::PortStatus));
		for (size_t i = 0; i < byte_count; i++)
			new_ports |= static_cast<uint32_t>(bitmap[i]) << (i * 8);

		if (new_ports)
		{
			m_changed_ports |= new_ports;
			m_changed_port_blocker.unblock();
		}
	}

}
