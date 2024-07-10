#pragma once

#include <BAN/NoCopyMove.h>

#include <kernel/USB/USBManager.h>
#include <kernel/USB/Controller.h>

namespace Kernel
{

	class USBDevice
	{
		BAN_NON_COPYABLE(USBDevice);
		BAN_NON_MOVABLE(USBDevice);

	public:
		struct EndpointDescriptor
		{
			USBEndpointDescriptor descriptor;
		};

		struct InterfaceDescriptor
		{
			USBInterfaceDescritor descriptor;
			BAN::Vector<EndpointDescriptor> endpoints;
		};

		struct ConfigurationDescriptor
		{
			USBConfigurationDescriptor desciptor;
			BAN::Vector<InterfaceDescriptor> interfaces;
		};

		struct DeviceDescriptor
		{
			USBDeviceDescriptor descriptor;
			BAN::Vector<ConfigurationDescriptor> configurations;
		};

	public:
		USBDevice() = default;
		virtual ~USBDevice() = default;

		BAN::ErrorOr<void> initialize();

		static USB::SpeedClass determine_speed_class(uint64_t bits_per_second);

	protected:
		virtual BAN::ErrorOr<void> initialize_control_endpoint() = 0;
		virtual BAN::ErrorOr<void> send_request(const USBDeviceRequest&, paddr_t buffer) = 0;

	private:
		DeviceDescriptor m_descriptor;
	};

}
