#include <BAN/Bitcast.h>
#include <BAN/ByteSpan.h>

#include <kernel/Lock/LockGuard.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/XHCI/Device.h>

#define DEBUG_XHCI 0

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<XHCIDevice>> XHCIDevice::create(XHCIController& controller, uint32_t port_id, uint32_t slot_id)
	{
		return TRY(BAN::UniqPtr<XHCIDevice>::create(controller, port_id, slot_id));
	}

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
		const uint32_t bits_per_second = m_controller.port(m_port_id).speed_id_to_speed[speed_id];
		const auto speed_class = determine_speed_class(bits_per_second);

		m_max_packet_size = 0;
		switch (speed_class)
		{
			case USB::SpeedClass::LowSpeed:
			case USB::SpeedClass::FullSpeed:
				m_max_packet_size = 8;
				break;
			case USB::SpeedClass::HighSpeed:
				m_max_packet_size = 64;
				break;
			case USB::SpeedClass::SuperSpeed:
				m_max_packet_size = 512;
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

			input_control_context.add_context_flags = 0b11;

			slot_context.root_hub_port_number = m_port_id;
			slot_context.route_string         = 0;
			slot_context.context_entries      = 1;
			slot_context.interrupter_target   = 0;
			slot_context.speed                = speed_id;

			endpoint0_context.endpoint_type       = XHCI::EndpointType::Control;
			endpoint0_context.max_packet_size     = m_max_packet_size;
			endpoint0_context.max_burst_size      = 0;
			endpoint0_context.tr_dequeue_pointer  = m_endpoints[0].transfer_ring->paddr() | 1;
			endpoint0_context.interval            = 0;
			endpoint0_context.max_primary_streams = 0;
			endpoint0_context.mult                = 0;
			endpoint0_context.error_count         = 3;
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

		// NOTE: Full speed devices can have other max packet sizes than 8
		if (speed_class == USB::SpeedClass::FullSpeed)
			TRY(update_actual_max_packet_size());

		return {};
	}

	BAN::ErrorOr<void> XHCIDevice::update_actual_max_packet_size()
	{
		dprintln_if(DEBUG_XHCI, "Retrieving actual max packet size of full speed device");

		BAN::Vector<uint8_t> buffer;
		TRY(buffer.resize(8, 0));

		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
		request.bRequest      = USB::Request::GET_DESCRIPTOR;
		request.wValue        = 0x0100;
		request.wIndex        = 0;
		request.wLength       = 8;
		TRY(send_request(request, kmalloc_paddr_of((vaddr_t)buffer.data()).value()));

		m_max_packet_size = buffer.back();

		const uint32_t context_size = m_controller.context_size_set() ? 64 : 32;

		{
			auto& input_control_context = *reinterpret_cast<XHCI::InputControlContext*>(m_input_context->vaddr() + 0 * context_size);
			auto& endpoint0_context     = *reinterpret_cast<XHCI::EndpointContext*>    (m_input_context->vaddr() + 2 * context_size);

			input_control_context.add_context_flags = 0b10;

			endpoint0_context.endpoint_type       = XHCI::EndpointType::Control;
			endpoint0_context.max_packet_size     = m_max_packet_size;
			endpoint0_context.max_burst_size      = 0;
			endpoint0_context.tr_dequeue_pointer  = (m_endpoints[0].transfer_ring->paddr() + (m_endpoints[0].enqueue_index * sizeof(XHCI::TRB))) | 1;
			endpoint0_context.interval            = 0;
			endpoint0_context.max_primary_streams = 0;
			endpoint0_context.mult                = 0;
			endpoint0_context.error_count         = 3;
		}

		XHCI::TRB evaluate_context { .address_device_command = {} };
		evaluate_context.address_device_command.trb_type                  = XHCI::TRBType::EvaluateContextCommand;
		evaluate_context.address_device_command.input_context_pointer     = m_input_context->paddr();
		evaluate_context.address_device_command.block_set_address_request = 0;
		evaluate_context.address_device_command.slot_id                   = m_slot_id;
		TRY(m_controller.send_command(evaluate_context));

		dprintln_if(DEBUG_XHCI, "successfully updated max packet size to {}", m_max_packet_size);

		return {};
	}

	void XHCIDevice::on_transfer_event(const volatile XHCI::TRB& trb)
	{
		ASSERT(trb.trb_type == XHCI::TRBType::TransferEvent);
		if (trb.transfer_event.endpoint_id == 0)
		{
			dwarnln("TransferEvent for endpoint id 0");
			return;
		}

		// NOTE: dword2 is last (and atomic) as that is what send_request is waiting for
		auto& completion_trb = m_endpoints[trb.transfer_event.endpoint_id - 1].completion_trb;
		completion_trb.raw.dword0 = trb.raw.dword0;
		completion_trb.raw.dword1 = trb.raw.dword1;
		completion_trb.raw.dword3 = trb.raw.dword3;
		__atomic_store_n(&completion_trb.raw.dword2, trb.raw.dword2, __ATOMIC_SEQ_CST);
	}

	BAN::ErrorOr<void> XHCIDevice::send_request(const USBDeviceRequest& request, paddr_t buffer_paddr)
	{
		// minus 3: Setup, Status, Link
		if (request.wLength > (m_transfer_ring_trb_count - 3) * m_max_packet_size)
			return BAN::Error::from_errno((ENOBUFS));

		auto& endpoint = m_endpoints[0];
		LockGuard _(endpoint.mutex);

		auto* transfer_trb_arr = reinterpret_cast<volatile XHCI::TRB*>(endpoint.transfer_ring->vaddr());

		{
			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset((void*)&trb, 0, sizeof(XHCI::TRB));

			trb.setup_stage.trb_type                = XHCI::TRBType::SetupStage;
			trb.setup_stage.transfer_type           = 3;
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

		uint32_t bytes_handled = 0;
		while (bytes_handled < request.wLength)
		{
			const uint32_t to_handle = BAN::Math::min<uint32_t>(m_max_packet_size, request.wLength - bytes_handled);

			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset((void*)&trb, 0, sizeof(XHCI::TRB));

			trb.data_stage.trb_type                = XHCI::TRBType::DataStage;
			trb.data_stage.direction               = 1;
			trb.data_stage.trb_transfer_length     = to_handle;
			trb.data_stage.chain_bit               = (bytes_handled + to_handle < request.wLength);
			trb.data_stage.interrupt_on_completion = 0;
			trb.data_stage.immediate_data          = 0;
			trb.data_stage.data_buffer_pointer     = buffer_paddr + bytes_handled;
			trb.data_stage.cycle_bit               = endpoint.cycle_bit;

			bytes_handled += to_handle;

			advance_endpoint_enqueue(endpoint, false);
		}

		{
			auto& trb = transfer_trb_arr[endpoint.enqueue_index];
			memset((void*)&trb, 0, sizeof(XHCI::TRB));

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

		m_controller.doorbell_reg(m_slot_id) = 1;

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 1000;
		while ((__atomic_load_n(&completion_trb.raw.dword2, __ATOMIC_SEQ_CST) >> 24) == 0)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		if (completion_trb.transfer_event.completion_code != 1)
		{
			dwarnln("Completion error: {}", +completion_trb.transfer_event.completion_code);
			return BAN::Error::from_errno(EFAULT);
		}

		return {};
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
