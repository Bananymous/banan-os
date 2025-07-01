#pragma once

#include <kernel/Process.h>
#include <kernel/USB/Device.h>

namespace Kernel
{

	class USBHubDriver final : public USBClassDriver
	{
	public:
		BAN::ErrorOr<void> initialize() override;

		void handle_stall(uint8_t endpoint_id) override;
		void handle_input_data(size_t byte_count, uint8_t endpoint_id) override;

	private:
		USBHubDriver(USBDevice&, const USBDevice::DeviceDescriptor&);
		~USBHubDriver();

		void port_updater_task();

		BAN::ErrorOr<void> clear_port_feature(uint8_t port, uint8_t feature);
		BAN::ErrorOr<void> set_port_feature(uint8_t port, uint8_t feature);

	private:
		USBDevice& m_device;
		const USBDevice::DeviceDescriptor& m_descriptor;

		BAN::UniqPtr<DMARegion> m_data_region;
		uint8_t m_endpoint_id { 0 };

		uint8_t m_port_count { 0 };

		BAN::Atomic<uint32_t> m_changed_ports { 0 };
		ThreadBlocker m_changed_port_blocker;
		BAN::Atomic<Thread*> m_port_updater { nullptr };

		struct PortInfo
		{
			uint8_t slot { 0 };
			bool failed { false };
		};
		BAN::Vector<PortInfo> m_ports;

		BAN::Atomic<bool> m_running { true };

		bool m_is_usb2 { false };
		bool m_is_usb3 { false };
		bool m_is_multi_tt { false };

		bool m_is_init_done { false };

		friend class BAN::UniqPtr<USBHubDriver>;
	};

}
