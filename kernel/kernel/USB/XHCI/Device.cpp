#include <BAN/Bitcast.h>
#include <BAN/ByteSpan.h>

#include <kernel/Lock/LockGuard.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/XHCI/Device.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<XHCIDevice>> XHCIDevice::create(XHCIController& controller, uint32_t port_id, uint32_t slot_id)
	{
		return TRY(BAN::UniqPtr<XHCIDevice>::create(controller, port_id, slot_id));
	}

	uint64_t XHCIDevice::calculate_port_bits_per_second(XHCIController& controller, uint32_t port_id)
	{
		const uint32_t portsc = controller.operational_regs().ports[port_id - 1].portsc;
		const uint32_t speed_id = (portsc >> XHCI::PORTSC::PORT_SPEED_SHIFT) & XHCI::PORTSC::PORT_SPEED_MASK;
		return controller.port(port_id).speed_id_to_speed[speed_id];
	}

	XHCIDevice::XHCIDevice(XHCIController& controller, uint32_t port_id, uint32_t slot_id)
		: USBDevice(determine_speed_class(calculate_port_bits_per_second(controller, port_id)))
		, m_controller(controller)
		, m_port_id(port_id)
		, m_slot_id(slot_id)
	{}

	XHCIDevice::~XHCIDevice()
	{
		XHCI::TRB disable_slot { .disable_slot_command {} };
		disable_slot.disable_slot_command.trb_type = XHCI::TRBType::DisableSlotCommand;
		disable_slot.disable_slot_command.slot_id = m_slot_id;
		if (auto ret = m_controller.send_command(disable_slot); ret.is_error())
			dwarnln("Could not disable slot {}: {}", m_slot_id, ret.error());
		else
			dprintln_if(DEBUG_XHCI, "Slot {} disabled", m_slot_id);
	}

	BAN::ErrorOr<void> XHCIDevice::initialize_control_endpoint()
	{
		const uint32_t context_size = m_controller.context_size_set() ? 64 : 32;

		const uint32_t portsc = m_controller.operational_regs().ports[m_port_id - 1].portsc;
		const uint32_t speed_id = (portsc >> XHCI::PORTSC::PORT_SPEED_SHIFT) & XHCI::PORTSC::PORT_SPEED_MASK;

		m_endpoints[0].max_packet_size = 0;
		switch (m_speed_class)
		{
			case USB::SpeedClass::LowSpeed:
			case USB::SpeedClass::FullSpeed:
				m_endpoints[0].max_packet_size = 8;
				break;
			case USB::SpeedClass::HighSpeed:
				m_endpoints[0].max_packet_size = 64;
				break;
			case USB::SpeedClass::SuperSpeed:
				m_endpoints[0].max_packet_size = 512;
				break;
			default: ASSERT_NOT_REACHED();
		}

		m_input_context = TRY(DMARegion::create(33 * context_size));
		memset(reinterpret_cast<void*>(m_input_context->vaddr()), 0, m_input_context->size());

		m_output_context = TRY(DMARegion::create(32 * context_size));
		memset(reinterpret_cast<void*>(m_output_context->vaddr()), 0, m_output_context->size());

		m_endpoints[0].transfer_ring = TRY(DMARegion::create(m_transfer_ring_trb_count * sizeof(XHCI::TRB)));
		memset(reinterpret_cast<void*>(m_endpoints[0].transfer_ring->vaddr()), 0, m_endpoints[0].transfer_ring->size());

		{
			auto& input_control_context = *reinterpret_cast<XHCI::InputControlContext*>(m_input_context->vaddr() + 0 * context_size);
			auto& slot_context          = *reinterpret_cast<XHCI::SlotContext*>        (m_input_context->vaddr() + 1 * context_size);
			auto& endpoint0_context     = *reinterpret_cast<XHCI::EndpointContext*>    (m_input_context->vaddr() + 2 * context_size);

			memset(&input_control_context, 0, context_size);
			input_control_context.add_context_flags = 0b11;

			memset(&slot_context, 0, context_size);
			slot_context.route_string         = 0;
			slot_context.root_hub_port_number = m_port_id;
			slot_context.context_entries      = 1;
			slot_context.interrupter_target   = 0;
			slot_context.speed                = speed_id;
			// FIXME: 4.5.2 hub

			memset(&endpoint0_context, 0, context_size);
			endpoint0_context.endpoint_type       = XHCI::EndpointType::Control;
			endpoint0_context.max_packet_size     = m_endpoints[0].max_packet_size;
			endpoint0_context.error_count         = 3;
			endpoint0_context.tr_dequeue_pointer  = m_endpoints[0].transfer_ring->paddr() | 1;
		}

		m_controller.dcbaa_reg(m_slot_id) = m_output_context->paddr();

		for (int i = 0; i < 2; i++)
		{
			XHCI::TRB address_device { .address_device_command = {} };
			address_device.address_device_command.trb_type                  = XHCI::TRBType::AddressDeviceCommand;
			address_device.address_device_command.input_context_pointer     = m_input_context->paddr();
			// NOTE: some legacy devices require sending request with BSR=1 before actual BSR=0
			address_device.address_device_command.block_set_address_request = (i == 0);
			address_device.address_device_command.slot_id                   = m_slot_id;
			TRY(m_controller.send_command(address_device));
		}

		TRY(update_actual_max_packet_size());

		return {};
	}

	BAN::ErrorOr<void> XHCIDevice::update_actual_max_packet_size()
	{
		// FIXME: This is more or less generic USB code

		dprintln_if(DEBUG_XHCI, "Retrieving actual max packet size");

		BAN::Vector<uint8_t> buffer;
		TRY(buffer.resize(8, 0));

		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
		request.bRequest      = USB::Request::GET_DESCRIPTOR;
		request.wValue        = 0x0100;
		request.wIndex        = 0;
		request.wLength       = 8;
		TRY(send_request(request, kmalloc_paddr_of((vaddr_t)buffer.data()).value()));

		const bool is_usb3 = m_controller.port(m_port_id).revision_major == 3;
		const uint32_t new_max_packet_size = is_usb3 ? 1u << buffer.back() : buffer.back();

		if (m_endpoints[0].max_packet_size == new_max_packet_size)
			return {};

		m_endpoints[0].max_packet_size = new_max_packet_size;

		const uint32_t context_size = m_controller.context_size_set() ? 64 : 32;

		{
			auto& input_control_context = *reinterpret_cast<XHCI::InputControlContext*>(m_input_context->vaddr() + 0 * context_size);
			auto& slot_context          = *reinterpret_cast<XHCI::SlotContext*>        (m_input_context->vaddr() + 1 * context_size);
			auto& endpoint0_context     = *reinterpret_cast<XHCI::EndpointContext*>    (m_input_context->vaddr() + 2 * context_size);

			memset(&input_control_context, 0, context_size);
			input_control_context.add_context_flags = 0b11;

			memset(&slot_context, 0, context_size);
			slot_context.max_exit_latency   = 0; // FIXME:
			slot_context.interrupter_target = 0;

			memset(&endpoint0_context, 0, context_size);
			endpoint0_context.endpoint_type       = XHCI::EndpointType::Control;
			endpoint0_context.max_packet_size     = m_endpoints[0].max_packet_size;
			endpoint0_context.error_count         = 3;
			endpoint0_context.tr_dequeue_pointer  = m_endpoints[0].transfer_ring->paddr() | 1;
		}

		XHCI::TRB evaluate_context { .address_device_command = {} };
		evaluate_context.address_device_command.trb_type                  = XHCI::TRBType::EvaluateContextCommand;
		evaluate_context.address_device_command.input_context_pointer     = m_input_context->paddr();
		evaluate_context.address_device_command.block_set_address_request = 0;
		evaluate_context.address_device_command.slot_id                   = m_slot_id;
		TRY(m_controller.send_command(evaluate_context));

		dprintln_if(DEBUG_XHCI, "successfully updated max packet size to {}", m_endpoints[0].max_packet_size);

		return {};
	}

	// 6.2.3.6 Interval
	static uint32_t determine_interval(const USBEndpointDescriptor& endpoint_descriptor, USB::SpeedClass speed_class)
	{
		auto ep_type = static_cast<USB::EndpointType>(endpoint_descriptor.bDescriptorType & 0x03);

		switch (speed_class)
		{
			case USB::SpeedClass::HighSpeed:
				// maximum NAK rate
				if (ep_type == USB::EndpointType::Control || ep_type == USB::EndpointType::Bulk)
					return (endpoint_descriptor.bInterval == 0) ? 0 : BAN::Math::clamp<uint32_t>(
						BAN::Math::ilog2<uint32_t>(endpoint_descriptor.bInterval), 0, 15
					);
				// fall through
			case USB::SpeedClass::SuperSpeed:
				if (ep_type == USB::EndpointType::Isochronous || ep_type == USB::EndpointType::Interrupt)
					return BAN::Math::clamp<uint32_t>(endpoint_descriptor.bInterval - 1, 0, 15);
				return 0;
			case USB::SpeedClass::FullSpeed:
				if (ep_type == USB::EndpointType::Isochronous)
					return BAN::Math::clamp<uint32_t>(endpoint_descriptor.bInterval + 2, 3, 18);
				// fall through
			case USB::SpeedClass::LowSpeed:
				if (ep_type == USB::EndpointType::Isochronous || ep_type == USB::EndpointType::Interrupt)
					return (endpoint_descriptor.bInterval == 0) ? 0 : BAN::Math::clamp<uint32_t>(
						BAN::Math::ilog2<uint32_t>(endpoint_descriptor.bInterval * 8), 3, 10
					);
				return 0;
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> XHCIDevice::initialize_endpoint(const USBEndpointDescriptor& endpoint_descriptor)
	{
		const bool is_control   { (endpoint_descriptor.bmAttributes & 0x03) == 0x00 };
		const bool is_isoch     { (endpoint_descriptor.bmAttributes & 0x03) == 0x01 };
		const bool is_bulk      { (endpoint_descriptor.bmAttributes & 0x03) == 0x02 };
		const bool is_interrupt { (endpoint_descriptor.bmAttributes & 0x03) == 0x03 };

		(void)is_control;
		(void)is_isoch;
		(void)is_bulk;
		(void)is_interrupt;

		XHCI::EndpointType endpoint_type;
		switch ((endpoint_descriptor.bEndpointAddress & 0x80) | (endpoint_descriptor.bmAttributes & 0x03))
		{
			case 0x00: ASSERT_NOT_REACHED();
			case 0x80: endpoint_type = XHCI::EndpointType::Control;      break;
			case 0x01: endpoint_type = XHCI::EndpointType::IsochOut;     break;
			case 0x81: endpoint_type = XHCI::EndpointType::IsochIn;      break;
			case 0x02: endpoint_type = XHCI::EndpointType::BulkOut;      break;
			case 0x82: endpoint_type = XHCI::EndpointType::BulkIn;       break;
			case 0x03: endpoint_type = XHCI::EndpointType::InterruptOut; break;
			case 0x83: endpoint_type = XHCI::EndpointType::InterruptIn;  break;
			default: ASSERT_NOT_REACHED();
		}

		// FIXME: Streams

		const uint32_t endpoint_id        = (endpoint_descriptor.bEndpointAddress & 0x0F) * 2 + !!(endpoint_descriptor.bEndpointAddress & 0x80);
		const uint32_t max_packet_size    = (is_control || is_bulk) ? endpoint_descriptor.wMaxPacketSize :  endpoint_descriptor.wMaxPacketSize & 0x07FF;
		const uint32_t max_burst_size     = (is_control || is_bulk) ? 0                                  : (endpoint_descriptor.wMaxPacketSize & 0x1800) >> 11;
		const uint32_t max_esit_payload   = max_packet_size * (max_burst_size + 1);
		const uint32_t interval           = determine_interval(endpoint_descriptor, m_speed_class);
		const uint32_t average_trb_length = (is_control) ? 8 : max_esit_payload;
		const uint32_t error_count        = (is_isoch)   ? 0 : 3;

		auto& endpoint = m_endpoints[endpoint_id - 1];
		ASSERT(!endpoint.transfer_ring);

		uint32_t last_valid_endpoint_id = endpoint_id;
		for (size_t i = endpoint_id; i < m_endpoints.size(); i++)
			if (m_endpoints[i].transfer_ring)
				last_valid_endpoint_id = i + 1;

		endpoint.transfer_ring   = TRY(DMARegion::create(m_transfer_ring_trb_count * sizeof(XHCI::TRB)));
		endpoint.max_packet_size = max_packet_size;
		endpoint.dequeue_index   = 0;
		endpoint.enqueue_index   = 0;
		endpoint.cycle_bit       = 1;
		endpoint.callback        = (is_interrupt || is_bulk) ? &XHCIDevice::on_interrupt_or_bulk_endpoint_event : nullptr;

		memset(reinterpret_cast<void*>(endpoint.transfer_ring->vaddr()), 0, endpoint.transfer_ring->size());

		{
			const uint32_t context_size = m_controller.context_size_set() ? 64 : 32;

			auto& input_control_context = *reinterpret_cast<XHCI::InputControlContext*>(m_input_context->vaddr());
			auto& slot_context          = *reinterpret_cast<XHCI::SlotContext*>        (m_input_context->vaddr() + context_size);
			auto& endpoint_context      = *reinterpret_cast<XHCI::EndpointContext*>    (m_input_context->vaddr() + (endpoint_id + 1) * context_size);

			memset(&input_control_context, 0, context_size);
			input_control_context.add_context_flags = (1u << endpoint_id) | 1;

			memset(&slot_context, 0, context_size);
			slot_context.context_entries = last_valid_endpoint_id;
			// FIXME: 4.5.2 hub

			memset(&endpoint_context, 0, context_size);
			endpoint_context.endpoint_type       = endpoint_type;
			endpoint_context.max_packet_size     = max_packet_size;
			endpoint_context.max_burst_size      = max_burst_size;
			endpoint_context.mult                = 0;
			endpoint_context.error_count         = error_count;
			endpoint_context.tr_dequeue_pointer  = endpoint.transfer_ring->paddr() | 1;
			endpoint_context.max_esit_payload_lo = max_esit_payload & 0xFFFF;
			endpoint_context.max_esit_payload_hi = max_esit_payload >> 16;
			endpoint_context.average_trb_length  = average_trb_length;
			endpoint_context.interval            = interval;
		}

		XHCI::TRB configure_endpoint { .configure_endpoint_command = {} };
		configure_endpoint.configure_endpoint_command.trb_type              = XHCI::TRBType::ConfigureEndpointCommand;
		configure_endpoint.configure_endpoint_command.input_context_pointer = m_input_context->paddr();
		configure_endpoint.configure_endpoint_command.deconfigure           = 0;
		configure_endpoint.configure_endpoint_command.slot_id               = m_slot_id;
		TRY(m_controller.send_command(configure_endpoint));

		return {};
	}

	void XHCIDevice::on_interrupt_or_bulk_endpoint_event(XHCI::TRB trb)
	{
		ASSERT(trb.trb_type == XHCI::TRBType::TransferEvent);

		const uint32_t endpoint_id = trb.transfer_event.endpoint_id;
		auto& endpoint = m_endpoints[endpoint_id - 1];

		if (trb.transfer_event.completion_code == 6)
			return handle_stall(endpoint_id);

		if (trb.transfer_event.completion_code != 1 && trb.transfer_event.completion_code != 13)
		{
			dwarnln("Interrupt or bulk endpoint got transfer event with completion code {}", +trb.transfer_event.completion_code);
			return;
		}

		const auto* transfer_trb_arr = reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr());
		const uint32_t transfer_trb_index = (trb.transfer_event.trb_pointer - endpoint.transfer_ring->paddr()) / sizeof(XHCI::TRB);
		const uint32_t original_len = transfer_trb_arr[transfer_trb_index].normal.trb_transfer_length;

		const uint32_t transfer_length = original_len - trb.transfer_event.trb_transfer_length;
		handle_input_data(transfer_length, endpoint_id);
	}

	void XHCIDevice::on_transfer_event(const volatile XHCI::TRB& trb)
	{
		ASSERT(trb.trb_type == XHCI::TRBType::TransferEvent);
		if (trb.transfer_event.endpoint_id == 0)
		{
			dwarnln("TransferEvent for endpoint id 0");
			return;
		}

		auto& endpoint = m_endpoints[trb.transfer_event.endpoint_id - 1];

		if (endpoint.callback)
		{
			XHCI::TRB copy;
			copy.raw.dword0 = trb.raw.dword0;
			copy.raw.dword1 = trb.raw.dword1;
			copy.raw.dword2 = trb.raw.dword2;
			copy.raw.dword3 = trb.raw.dword3;
			(this->*endpoint.callback)(copy);
			return;
		}

		// Get received bytes from short packet
		if (trb.transfer_event.completion_code == 13)
		{
			auto* transfer_trb_arr = reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr());

			const uint32_t trb_index = (trb.transfer_event.trb_pointer - endpoint.transfer_ring->paddr()) / sizeof(XHCI::TRB);

			const uint32_t full_trbs_transferred = (trb_index >= endpoint.dequeue_index)
				? trb_index                             - 1 - endpoint.dequeue_index
				: trb_index + m_transfer_ring_trb_count - 2 - endpoint.dequeue_index;

			const uint32_t full_trb_data = full_trbs_transferred * endpoint.max_packet_size;
			const uint32_t short_data    = transfer_trb_arr[trb_index].data_stage.trb_transfer_length - trb.transfer_event.trb_transfer_length;

			endpoint.transfer_count = full_trb_data + short_data;

			ASSERT(trb_index >= endpoint.dequeue_index);
			return;
		}

		// NOTE: dword2 is last (and atomic) as that is what send_request is waiting for
		auto& completion_trb = endpoint.completion_trb;
		completion_trb.raw.dword0 = trb.raw.dword0;
		completion_trb.raw.dword1 = trb.raw.dword1;
		completion_trb.raw.dword3 = trb.raw.dword3;
		__atomic_store_n(&completion_trb.raw.dword2, trb.raw.dword2, __ATOMIC_SEQ_CST);
	}

	BAN::ErrorOr<size_t> XHCIDevice::send_request(const USBDeviceRequest& request, paddr_t buffer_paddr)
	{
		// FIXME: This is more or less generic USB code

		auto& endpoint = m_endpoints[0];

		// minus 3: Setup, Status, Link (this is probably too generous and will result in STALL)
		if (request.wLength > (m_transfer_ring_trb_count - 3) * endpoint.max_packet_size)
			return BAN::Error::from_errno((ENOBUFS));

		LockGuard _(endpoint.mutex);

		uint8_t transfer_type =
			[&request]() -> uint8_t
			{
				if (request.wLength == 0)
					return 0;
				if (request.bmRequestType & USB::RequestType::DeviceToHost)
					return 3;
				return 2;
			}();

		auto* transfer_trb_arr = reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr());

		{
			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset(const_cast<XHCI::TRB*>(&trb), 0, sizeof(XHCI::TRB));

			trb.setup_stage.trb_type                = XHCI::TRBType::SetupStage;
			trb.setup_stage.transfer_type           = transfer_type;
			trb.setup_stage.trb_transfer_length     = 8;
			trb.setup_stage.interrupt_on_completion = 0;
			trb.setup_stage.immediate_data          = 1;
			trb.setup_stage.cycle_bit               = endpoint.cycle_bit;

			trb.setup_stage.bmRequestType = request.bmRequestType;
			trb.setup_stage.bRequest      = request.bRequest;
			trb.setup_stage.wValue        = request.wValue;
			trb.setup_stage.wIndex        = request.wIndex;
			trb.setup_stage.wLength       = request.wLength;

			advance_endpoint_enqueue(endpoint, false);
		}

		const uint32_t td_packet_count = BAN::Math::div_round_up<uint32_t>(request.wLength, endpoint.max_packet_size);
		uint32_t packets_transferred = 1;

		uint32_t bytes_handled = 0;
		while (bytes_handled < request.wLength)
		{
			const uint32_t to_handle = BAN::Math::min<uint32_t>(endpoint.max_packet_size, request.wLength - bytes_handled);

			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset(const_cast<XHCI::TRB*>(&trb), 0, sizeof(XHCI::TRB));

			trb.data_stage.trb_type                  = XHCI::TRBType::DataStage;
			trb.data_stage.direction                 = 1;
			trb.data_stage.trb_transfer_length       = to_handle;
			trb.data_stage.td_size                   = BAN::Math::min<uint32_t>(td_packet_count - packets_transferred, 31);
			trb.data_stage.chain_bit                 = (bytes_handled + to_handle < request.wLength);
			trb.data_stage.interrupt_on_completion   = 0;
			trb.data_stage.interrupt_on_short_packet = 1;
			trb.data_stage.immediate_data            = 0;
			trb.data_stage.data_buffer_pointer       = buffer_paddr + bytes_handled;
			trb.data_stage.cycle_bit                 = endpoint.cycle_bit;

			bytes_handled += to_handle;
			packets_transferred++;

			advance_endpoint_enqueue(endpoint, trb.data_stage.chain_bit);
		}

		{
			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset(const_cast<XHCI::TRB*>(&trb), 0, sizeof(XHCI::TRB));

			trb.status_stage.trb_type                = XHCI::TRBType::StatusStage;
			trb.status_stage.direction               = 0;
			trb.status_stage.chain_bit               = 0;
			trb.status_stage.interrupt_on_completion = 1;
			trb.data_stage.cycle_bit                 = endpoint.cycle_bit;

			advance_endpoint_enqueue(endpoint, false);
		}

		auto& completion_trb = endpoint.completion_trb;
		completion_trb.raw.dword0 = 0;
		completion_trb.raw.dword1 = 0;
		completion_trb.raw.dword2 = 0;
		completion_trb.raw.dword3 = 0;

		endpoint.transfer_count = request.wLength;

		m_controller.doorbell_reg(m_slot_id) = 1;

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 1000;
		while ((__atomic_load_n(&completion_trb.raw.dword2, __ATOMIC_SEQ_CST) >> 24) == 0)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		endpoint.dequeue_index = endpoint.enqueue_index;

		if (completion_trb.transfer_event.completion_code != 1)
		{
			dwarnln("Completion error: {}", +completion_trb.transfer_event.completion_code);
			return BAN::Error::from_errno(EFAULT);
		}

		return endpoint.transfer_count;
	}

	void XHCIDevice::send_data_buffer(uint8_t endpoint_id, paddr_t buffer, size_t buffer_len)
	{
		ASSERT(endpoint_id != 0);
		auto& endpoint = m_endpoints[endpoint_id - 1];

		ASSERT(buffer_len <= endpoint.max_packet_size);

		auto& trb = *reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr() + endpoint.enqueue_index * sizeof(XHCI::TRB));
		memset(const_cast<XHCI::TRB*>(&trb), 0, sizeof(XHCI::TRB));
		trb.normal.trb_type                  = XHCI::TRBType::Normal;
		trb.normal.data_buffer_pointer       = buffer;
		trb.normal.trb_transfer_length       = buffer_len;
		trb.normal.td_size                   = 0;
		trb.normal.interrupt_target          = 0;
		trb.normal.cycle_bit                 = endpoint.cycle_bit;
		trb.normal.interrupt_on_completion   = 1;
		trb.normal.interrupt_on_short_packet = 1;
		advance_endpoint_enqueue(endpoint, false);

		m_controller.doorbell_reg(m_slot_id) = endpoint_id;
	}

	void XHCIDevice::advance_endpoint_enqueue(Endpoint& endpoint, bool chain)
	{
		endpoint.enqueue_index++;
		if (endpoint.enqueue_index < m_transfer_ring_trb_count - 1)
			return;

		// This is the last TRB in transfer ring. Make it link to the beginning of the ring
		auto& link_trb = reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr())[endpoint.enqueue_index].link_trb;
		link_trb.trb_type                = XHCI::TRBType::Link;
		link_trb.ring_segment_ponter     = endpoint.transfer_ring->paddr();
		link_trb.interrupter_target      = 0;
		link_trb.cycle_bit               = endpoint.cycle_bit;
		link_trb.toggle_cycle            = 1;
		link_trb.chain_bit               = chain;
		link_trb.interrupt_on_completion = 0;

		endpoint.enqueue_index = 0;
		endpoint.cycle_bit = !endpoint.cycle_bit;
	}

}
