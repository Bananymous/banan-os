#pragma once

#include <BAN/NoCopyMove.h>

#include <kernel/Memory/DMARegion.h>
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
			USBInterfaceDescriptor descriptor;
			BAN::Vector<EndpointDescriptor> endpoints;
			BAN::Vector<BAN::Vector<uint8_t>> misc_descriptors;
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

		const BAN::Vector<ConfigurationDescriptor>& configurations() { return m_descriptor.configurations; }

		virtual BAN::ErrorOr<size_t> send_request(const USBDeviceRequest&, paddr_t buffer) = 0;

		static USB::SpeedClass determine_speed_class(uint64_t bits_per_second);

	protected:
		virtual BAN::ErrorOr<void> initialize_control_endpoint() = 0;

	private:
		BAN::ErrorOr<ConfigurationDescriptor> parse_configuration(size_t index);

	private:
		DeviceDescriptor m_descriptor;
		BAN::UniqPtr<DMARegion> m_dma_buffer;
	};

}
