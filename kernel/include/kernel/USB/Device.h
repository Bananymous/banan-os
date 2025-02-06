#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/NoCopyMove.h>

#include <kernel/Memory/DMARegion.h>
#include <kernel/USB/Controller.h>
#include <kernel/USB/USBManager.h>

namespace Kernel
{

	class USBClassDriver
	{
		BAN_NON_COPYABLE(USBClassDriver);
		BAN_NON_MOVABLE(USBClassDriver);

	public:
		USBClassDriver() = default;
		virtual ~USBClassDriver() = default;

		virtual BAN::ErrorOr<void> initialize() { return {}; };

		virtual void handle_stall(uint8_t endpoint_id) = 0;
		virtual void handle_input_data(size_t byte_count, uint8_t endpoint_id) = 0;
	};

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
		USBDevice(USB::SpeedClass speed_class)
			: m_speed_class(speed_class)
		{}
		virtual ~USBDevice() = default;

		// Class drivers have to be destroyed before derived class destructor is called
		void destroy() { m_class_drivers.clear(); }

		BAN::ErrorOr<void> initialize();

		const BAN::Vector<ConfigurationDescriptor>& configurations() { return m_descriptor.configurations; }

		virtual BAN::ErrorOr<void> initialize_endpoint(const USBEndpointDescriptor&) = 0;
		virtual BAN::ErrorOr<size_t> send_request(const USBDeviceRequest&, paddr_t buffer) = 0;
		virtual void send_data_buffer(uint8_t endpoint_id, paddr_t buffer, size_t buffer_len) = 0;

		USB::SpeedClass speed_class() const { return m_speed_class; }
		uint8_t depth() const { return m_depth; }

	protected:
		void handle_stall(uint8_t endpoint_id);
		void handle_input_data(size_t byte_count, uint8_t endpoint_id);
		virtual BAN::ErrorOr<void> initialize_control_endpoint() = 0;

	private:
		BAN::ErrorOr<ConfigurationDescriptor> parse_configuration(size_t index);

	protected:
		const USB::SpeedClass m_speed_class;

	private:
		DeviceDescriptor m_descriptor;
		BAN::UniqPtr<DMARegion> m_dma_buffer;

		BAN::Vector<BAN::UniqPtr<USBClassDriver>> m_class_drivers;
	};

}
