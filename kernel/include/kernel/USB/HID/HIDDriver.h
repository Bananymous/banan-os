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

		virtual BAN::ErrorOr<void> initialize() { return {}; }

		virtual void start_report() = 0;
		virtual void stop_report() = 0;

		virtual void handle_array(uint16_t usage_page, uint16_t usage) = 0;
		virtual void handle_variable(uint16_t usage_page, uint16_t usage, int64_t state) = 0;
		virtual void handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max) = 0;
	};

	class USBHIDDriver final : public USBClassDriver
	{
		BAN_NON_COPYABLE(USBHIDDriver);
		BAN_NON_MOVABLE(USBHIDDriver);

	public:
		struct DeviceReport
		{
			BAN::Vector<USBHID::Report> inputs;
			BAN::Vector<USBHID::Report> outputs;
			BAN::RefPtr<USBHIDDevice> device;
		};

	public:
		void handle_stall(uint8_t endpoint_id) override;
		void handle_input_data(size_t byte_count, uint8_t endpoint_id) override;

		bool is_hid_driver() const override { return true; }

		USBDevice& device() { return m_device; }
		const USBDevice::InterfaceDescriptor& interface() const { return m_interface; }

		bool has_led_control() const { return !m_led_controls.empty(); }
		void set_leds(uint32_t led_mask);

	private:
		USBHIDDriver(USBDevice&, const USBDevice::InterfaceDescriptor&);
		~USBHIDDriver();

		BAN::ErrorOr<void> initialize() override;

		BAN::ErrorOr<void> initializes_device_reports(const BAN::Vector<USBHID::Collection>&);

	private:
		struct LEDControl
		{
			DeviceReport* report;
			uint32_t report_id;
			uint32_t report_bits;
		};

	private:
		USBDevice& m_device;
		USBDevice::InterfaceDescriptor m_interface;

		bool m_uses_report_id { false };
		BAN::Vector<DeviceReport> m_device_reports;

		uint8_t m_data_endpoint_id = 0;
		BAN::UniqPtr<DMARegion> m_data_buffer;

		BAN::Vector<LEDControl> m_led_controls;
		BAN::UniqPtr<DMARegion> m_led_region;

		friend class BAN::UniqPtr<USBHIDDriver>;
	};

}
