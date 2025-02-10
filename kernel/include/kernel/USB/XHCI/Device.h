#pragma once

#include <BAN/Array.h>
#include <BAN/RefPtr.h>

#include <kernel/USB/Device.h>
#include <kernel/USB/XHCI/Controller.h>

namespace Kernel
{

	class XHCIDevice final : public USBDevice
	{
		BAN_NON_COPYABLE(XHCIDevice);
		BAN_NON_MOVABLE(XHCIDevice);

	public:
		struct Endpoint
		{
			BAN::UniqPtr<DMARegion> transfer_ring;
			uint32_t max_packet_size { 0 };
			uint32_t dequeue_index { 0 };
			uint32_t enqueue_index { 0 };
			bool cycle_bit { 1 };

			Mutex mutex;
			volatile uint32_t transfer_count { 0 };
			volatile XHCI::TRB completion_trb;

			void(XHCIDevice::*callback)(XHCI::TRB);
		};

		struct Info
		{
			XHCIDevice* parent_hub;
			uint8_t parent_port_id;
			USB::SpeedClass speed_class;
			uint8_t slot_id;
			uint32_t route_string;
			uint8_t depth;
		};

	public:
		static BAN::ErrorOr<BAN::UniqPtr<XHCIDevice>> create(XHCIController&, const Info& info);

		BAN::ErrorOr<uint8_t> initialize_device_on_hub_port(uint8_t port_id, USB::SpeedClass) override;
		void deinitialize_device_slot(uint8_t slot_id) override;

		BAN::ErrorOr<void> configure_endpoint(const USBEndpointDescriptor&, const HubInfo&) override;
		BAN::ErrorOr<size_t> send_request(const USBDeviceRequest&, paddr_t buffer) override;
		void send_data_buffer(uint8_t endpoint_id, paddr_t buffer, size_t buffer_size) override;

		void on_transfer_event(const volatile XHCI::TRB&);

	protected:
		BAN::ErrorOr<void> initialize_control_endpoint() override;

	private:
		XHCIDevice(XHCIController& controller, const Info& info);
		~XHCIDevice();

		BAN::ErrorOr<void> update_actual_max_packet_size();

		bool is_multi_tt() const;

		void on_interrupt_or_bulk_endpoint_event(XHCI::TRB);

		void advance_endpoint_enqueue(Endpoint&, bool chain);

	private:
		static constexpr uint32_t m_transfer_ring_trb_count = PAGE_SIZE / sizeof(XHCI::TRB);

		XHCIController& m_controller;
		Info m_info;

		Mutex m_mutex;

		BAN::UniqPtr<DMARegion> m_input_context;
		BAN::UniqPtr<DMARegion> m_output_context;

		BAN::Array<Endpoint, 31> m_endpoints;

		friend class BAN::UniqPtr<XHCIDevice>;
	};

}
