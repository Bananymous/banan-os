#pragma once

#include <kernel/Input/InputDevice.h>
#include <kernel/USB/Device.h>

namespace Kernel
{

	namespace USBHID
	{

		struct Report
		{
			enum class Type { Input, Output, Feature };

			uint16_t usage_page;
			uint16_t usage_id;
			Type type;

			uint8_t report_id;
			uint32_t report_count;
			uint32_t report_size;

			uint32_t usage_minimum;
			uint32_t usage_maximum;

			int64_t logical_minimum;
			int64_t logical_maximum;

			int64_t physical_minimum;
			int64_t physical_maximum;

			uint8_t flags;
		};

		struct Collection
		{
			uint16_t usage_page;
			uint16_t usage_id;
			uint8_t type;

			BAN::Vector<BAN::Variant<Collection, Report>> entries;
		};

	}

	class USBHIDDevice : public InputDevice
	{
		BAN_NON_COPYABLE(USBHIDDevice);
		BAN_NON_MOVABLE(USBHIDDevice);

	public:
		USBHIDDevice(InputDevice::Type type)
			: InputDevice(type)
		{}
		virtual ~USBHIDDevice() = default;

		virtual void start_report() = 0;
		virtual void stop_report() = 0;

		virtual void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) = 0;
		virtual void handle_array(uint16_t usage_page, uint16_t usage) = 0;
	};

	class USBHIDDriver final : public USBClassDriver
	{
		BAN_NON_COPYABLE(USBHIDDriver);
		BAN_NON_MOVABLE(USBHIDDriver);

	public:
		struct DeviceReport
		{
			BAN::Vector<USBHID::Report> inputs;
			BAN::RefPtr<USBHIDDevice> device;
		};

	public:
		static BAN::ErrorOr<BAN::UniqPtr<USBHIDDriver>> create(USBDevice&, const USBDevice::InterfaceDescriptor&, uint8_t interface_index);

		void handle_input_data(BAN::ConstByteSpan, uint8_t endpoint_id) override;

	private:
		USBHIDDriver(USBDevice&, const USBDevice::InterfaceDescriptor&, uint8_t interface_index);
		~USBHIDDriver();

		BAN::ErrorOr<void> initialize();

		BAN::ErrorOr<BAN::Vector<DeviceReport>> initializes_device_reports(const BAN::Vector<USBHID::Collection>&);

	private:
		USBDevice& m_device;
		USBDevice::InterfaceDescriptor m_interface;
		const uint8_t m_interface_index;

		bool m_uses_report_id { false };

		uint8_t m_endpoint_id { 0 };
		BAN::Vector<DeviceReport> m_device_inputs;

		friend class BAN::UniqPtr<USBHIDDriver>;
	};

}
