#include <BAN/Bitcast.h>
#include <BAN/StringView.h>

#include <kernel/Lock/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/XHCI/Controller.h>
#include <kernel/USB/XHCI/Device.h>

namespace Kernel
{

	XHCIController::XHCIController(PCI::Device& pci_device)
		: m_pci_device(pci_device)
	{ }

	XHCIController::~XHCIController()
	{
		m_running = false;
		while (m_port_updater)
			Processor::yield();

		for (auto paddr : m_scratchpad_buffers)
			if (paddr)
				Heap::get().release_page(paddr);
	}

	BAN::ErrorOr<void> XHCIController::take_ownership(PCI::Device& pci_device)
	{
		auto bar = TRY(pci_device.allocate_bar_region(0));
		if (bar->type() != PCI::BarType::MEM)
		{
			dwarnln("XHCI controller with non-memory configuration space");
			return BAN::Error::from_errno(EINVAL);
		}

		auto& capabilities = *reinterpret_cast<volatile XHCI::CapabilityRegs*>(bar->vaddr());
		const uint16_t ext_offset = capabilities.hccparams1.xhci_extended_capabilities_pointer;
		if (ext_offset == 0)
		{
			dwarnln("XHCI controller does not have extended capabilities");
			return BAN::Error::from_errno(EFAULT);
		}

		vaddr_t ext_addr = bar->vaddr() + ext_offset * 4;
		while (true)
		{
			auto& ext_cap = *reinterpret_cast<volatile XHCI::ExtendedCap*>(ext_addr);

			if (ext_cap.capability_id == XHCI::ExtendedCapabilityID::USBLegacySupport)
			{
				auto& legacy = *reinterpret_cast<volatile XHCI::USBLegacySupportCap*>(ext_addr);
				if (!legacy.hc_bios_owned_semaphore)
					return {};
				legacy.hc_os_owned_semaphore = 1;

				const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 1000;
				while (legacy.hc_bios_owned_semaphore)
					if (SystemTimer::get().ms_since_boot() > timeout_ms)
						return BAN::Error::from_errno(ETIMEDOUT);
				return {};
			}

			if (ext_cap.next_capability == 0)
				break;
			ext_addr += ext_cap.next_capability * 4;
		}

		return {};
	}

	BAN::ErrorOr<BAN::UniqPtr<XHCIController>> XHCIController::create(PCI::Device& pci_device)
	{
		auto controller = TRY(BAN::UniqPtr<XHCIController>::create(pci_device));
		TRY(controller->initialize_impl());
		return controller;
	}

	BAN::ErrorOr<void> XHCIController::initialize_impl()
	{
		dprintln("XHCI controller at PCI {2H}:{2H}:{2H}", m_pci_device.bus(), m_pci_device.dev(), m_pci_device.func());

		m_pci_device.enable_bus_mastering();
		m_pci_device.enable_memory_space();

		m_configuration_bar = TRY(m_pci_device.allocate_bar_region(0));
		if (m_configuration_bar->type() != PCI::BarType::MEM)
		{
			dwarnln("XHCI controller with non-memory configuration space");
			return BAN::Error::from_errno(EINVAL);
		}

		if (auto ret = reset_controller(); ret.is_error())
		{
			dwarnln("Could not reset XHCI Controller: {}", ret.error());
			return ret.release_error();
		}

		auto& capabilities = capability_regs();
		dprintln("  version {H}.{H}.{H}",
			+capabilities.major_revision,
			capabilities.minor_revision >> 4,
			capabilities.minor_revision & 0x0F
		);
		dprintln("  max slots {}", +capabilities.hcsparams1.max_slots);
		dprintln("  max intrs {}", +capabilities.hcsparams1.max_interrupters);
		dprintln("  max ports {}", +capabilities.hcsparams1.max_ports);

		TRY(m_slots.resize(capabilities.hcsparams1.max_slots));
		TRY(m_ports.resize(capabilities.hcsparams1.max_ports));

		TRY(initialize_ports());

		auto& operational = operational_regs();

		// allocate and program dcbaa
		m_dcbaa_region = TRY(DMARegion::create(capabilities.hcsparams1.max_slots * 8));
		memset(reinterpret_cast<void*>(m_dcbaa_region->vaddr()), 0, m_dcbaa_region->size());
		operational.dcbaap_lo = m_dcbaa_region->paddr() & 0xFFFFFFFF;
		operational.dcbaap_hi = m_dcbaa_region->paddr() >> 32;

		// allocate and program crcr
		TRY(m_command_completions.resize(m_command_ring_trb_count));
		m_command_ring_region = TRY(DMARegion::create(m_command_ring_trb_count * sizeof(XHCI::TRB)));
		memset(reinterpret_cast<void*>(m_command_ring_region->vaddr()), 0, m_command_ring_region->size());
		operational.crcr_lo = m_command_ring_region->paddr() | XHCI::CRCR::RingCycleState;
		operational.crcr_hi = m_command_ring_region->paddr() >> 32;

		TRY(initialize_primary_interrupter());

		TRY(initialize_scratchpad());

		// enable the controller
		operational.usbcmd.run_stop = 1;
		while (operational.usbsts & XHCI::USBSTS::HCHalted)
			continue;

		m_port_updater = TRY(Thread::create_kernel([](void* data) { reinterpret_cast<XHCIController*>(data)->port_updater_task(); }, this));
		TRY(Processor::scheduler().add_thread(m_port_updater));

		return {};
	}

	BAN::ErrorOr<void> XHCIController::initialize_ports()
	{
		auto& capabilities = capability_regs();
		uint8_t max_ports = capabilities.hcsparams1.max_ports;

		ASSERT(m_ports.size() == max_ports);

		bool overrides_speed_ids = false;

		{
			uint16_t ext_offset = capabilities.hccparams1.xhci_extended_capabilities_pointer;
			if (ext_offset == 0)
			{
				dwarnln("XHCI controller does not have extended capabilities");
				return BAN::Error::from_errno(EFAULT);
			}

			vaddr_t ext_addr = m_configuration_bar->vaddr() + ext_offset * 4;
			while (true)
			{
				const auto& ext_cap = *reinterpret_cast<volatile XHCI::ExtendedCap*>(ext_addr);

				if (ext_cap.capability_id == XHCI::ExtendedCapabilityID::SupportedProtocol)
				{
					const auto& protocol = reinterpret_cast<const volatile XHCI::SupportedPrococolCap&>(ext_cap);

					const uint32_t target_name_string {
						('U' <<  0) |
						('S' <<  8) |
						('B' << 16) |
						(' ' << 24)
					};
					if (protocol.name_string != target_name_string)
					{
						dwarnln("Invalid port protocol name string");
						return BAN::Error::from_errno(EFAULT);
					}

					if (protocol.compatible_port_offset == 0 || protocol.compatible_port_offset + protocol.compatible_port_count - 1 > max_ports)
					{
						dwarnln("Invalid port specified in SupportedProtocols");
						return BAN::Error::from_errno(EFAULT);
					}

					if (protocol.protocol_slot_type != 0)
					{
						dwarnln("Invalid slot type specified in SupportedProtocols");
						return BAN::Error::from_errno(EFAULT);
					}

					if (protocol.protocol_speed_id_count != 0)
						overrides_speed_ids = true;

					for (size_t i = 0; i < protocol.compatible_port_count; i++)
					{
						auto& port = m_ports[protocol.compatible_port_offset + i - 1];
						port.revision_major = protocol.major_revision;
						port.revision_minor = protocol.minor_revision;
					}
				}

				if (ext_cap.next_capability == 0)
					break;
				ext_addr += ext_cap.next_capability * 4;
			}
		}

		if (overrides_speed_ids)
			dwarnln("xHCI overrides default speed ids, USB may not work");

		// set max slots enabled
		auto& operational = operational_regs();
		operational.config.max_device_slots_enabled = capabilities.hcsparams1.max_slots;

		return {};
	}

	uint8_t XHCIController::speed_class_to_id(USB::SpeedClass speed_class) const
	{
		switch (speed_class)
		{
			case USB::SpeedClass::LowSpeed:   return 2;
			case USB::SpeedClass::FullSpeed:  return 1;
			case USB::SpeedClass::HighSpeed:  return 3;
			case USB::SpeedClass::SuperSpeed: return 4;
		}
		ASSERT_NOT_REACHED();
	}

	USB::SpeedClass XHCIController::speed_id_to_class(uint8_t speed_id) const
	{
		switch (speed_id)
		{
			case 2: return USB::SpeedClass::LowSpeed;
			case 1: return USB::SpeedClass::FullSpeed;
			case 3: return USB::SpeedClass::HighSpeed;
			case 4: return USB::SpeedClass::SuperSpeed;
		}
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> XHCIController::initialize_primary_interrupter()
	{
		TRY(m_pci_device.reserve_interrupts(1));

		auto& runtime = runtime_regs();

		static constexpr size_t event_ring_table_offset = m_event_ring_trb_count * sizeof(XHCI::TRB);

		m_event_ring_region = TRY(DMARegion::create(m_event_ring_trb_count * sizeof(XHCI::TRB) + sizeof(XHCI::EventRingTableEntry)));
		memset(reinterpret_cast<void*>(m_event_ring_region->vaddr()), 0, m_event_ring_region->size());

		auto& event_ring_table_entry = *reinterpret_cast<XHCI::EventRingTableEntry*>(m_event_ring_region->vaddr() + event_ring_table_offset);
		event_ring_table_entry.rsba = m_event_ring_region->paddr();
		event_ring_table_entry.rsz = m_event_ring_trb_count;

		auto& primary_interrupter = runtime.irs[0];
		primary_interrupter.erstsz = (primary_interrupter.erstsz & 0xFFFF0000) | 1;
		primary_interrupter.erdp = m_event_ring_region->paddr() | XHCI::ERDP::EventHandlerBusy;
		primary_interrupter.erstba = m_event_ring_region->paddr() + event_ring_table_offset;

		auto& operational = operational_regs();
		operational.usbcmd.interrupter_enable = 1;

		primary_interrupter.iman = primary_interrupter.iman | XHCI::IMAN::InterruptPending | XHCI::IMAN::InterruptEnable;

		m_pci_device.enable_interrupt(0, *this);

		return {};
	}

	BAN::ErrorOr<void> XHCIController::initialize_scratchpad()
	{
		auto& capabilities = capability_regs();

		const uint32_t max_scratchpads = (capabilities.hcsparams2.max_scratchpad_buffers_hi << 5) | capabilities.hcsparams2.max_scratchpad_buffers_lo;
		if (max_scratchpads == 0)
			return {};

		m_scratchpad_buffer_array = TRY(DMARegion::create(max_scratchpads * sizeof(uint64_t)));
		TRY(m_scratchpad_buffers.resize(max_scratchpads));

		auto* scratchpad_buffer_array = reinterpret_cast<uint64_t*>(m_scratchpad_buffer_array->vaddr());
		for (size_t i = 0; i < max_scratchpads; i++)
		{
			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			m_scratchpad_buffers[i] = paddr;
			scratchpad_buffer_array[i] = paddr;
		}

		ASSERT(m_dcbaa_region);
		*reinterpret_cast<uint64_t*>(m_dcbaa_region->vaddr()) = m_scratchpad_buffer_array->paddr();

		return {};
	}

	void XHCIController::port_updater_task()
	{
		// allow initial pass of port iteration because controller
		// does not send Port Status Change event for already
		// attached ports
		m_port_changed = true;

		uint64_t last_port_update = SystemTimer::get().ms_since_boot();

		while (m_running)
		{
			{
				bool expected { true };
				while (!m_port_changed.compare_exchange(expected, false))
				{
					// If there has been no port activity in 100 ms, mark root hub as initialized
					if (!m_ports_initialized && SystemTimer::get().ms_since_boot() - last_port_update >= 100)
					{
						mark_hub_init_done(0);
						m_ports_initialized = true;
					}

					// FIXME: prevent race condition
					m_port_thread_blocker.block_with_timeout_ms(100, nullptr);
					expected = true;
				}
			}

			last_port_update = SystemTimer::get().ms_since_boot();;

			for (size_t i = 0; i < m_ports.size(); i++)
			{
				auto& my_port = m_ports[i];
				if (my_port.revision_major == 0)
					continue;

				auto& op_port = operational_regs().ports[i];

				if (!(op_port.portsc & XHCI::PORTSC::PP))
					continue;

				// read and clear needed change flags
				const bool reset_change      = op_port.portsc & XHCI::PORTSC::PRC;
				const bool connection_change = op_port.portsc & XHCI::PORTSC::CSC;
				const bool port_enabled      = op_port.portsc & XHCI::PORTSC::PED;
				op_port.portsc = XHCI::PORTSC::CSC | XHCI::PORTSC::PRC | XHCI::PORTSC::PP;

				if (!(op_port.portsc & XHCI::PORTSC::CCS))
				{
					if (my_port.slot_id != 0)
					{
						deinitialize_slot(my_port.slot_id);
						my_port.slot_id = 0;
					}
					continue;
				}

				switch (my_port.revision_major)
				{
					case 2:
						// USB2 ports advance to Enabled state after a reset
						if (port_enabled && reset_change)
							break;
						// reset port
						if (connection_change)
							op_port.portsc = XHCI::PORTSC::PR | XHCI::PORTSC::PP;
						continue;
					case 3:
						if (!connection_change || !port_enabled)
							continue;
						// USB3 ports advance to Enabled state automatically
						break;
					default:
						continue;
				}

				const uint8_t speed_id = (op_port.portsc >> XHCI::PORTSC::PORT_SPEED_SHIFT) & XHCI::PORTSC::PORT_SPEED_MASK;
				if (auto temp = speed_class_to_id(speed_id_to_class(speed_id)); temp != speed_id)
					dwarnln("FIXME: Using incorrect speed id, {} instead of {}", temp, speed_id);
				if (auto ret = initialize_device(i + 1, 0, speed_id_to_class(speed_id), nullptr, 0); !ret.is_error())
					my_port.slot_id = ret.value();
				else
				{
					dwarnln("Could not initialize USB {H}.{H} device: {}",
						my_port.revision_major,
						my_port.revision_minor >> 4,
						ret.error()
					);
				}
			}
		}

		m_port_updater = nullptr;
	}

	BAN::ErrorOr<uint8_t> XHCIController::initialize_device(uint32_t route_string, uint8_t depth, USB::SpeedClass speed_class, XHCIDevice* parent_hub, uint8_t parent_port_id)
	{
		XHCI::TRB enable_slot { .enable_slot_command {} };
		enable_slot.enable_slot_command.trb_type  = XHCI::TRBType::EnableSlotCommand;
		// 7.2.2.1.4: The Protocol Slot Type field of a USB3 or USB2 xHCI Supported Protocol Capability shall be set to ‘0’.
		enable_slot.enable_slot_command.slot_type = 0;
		auto result = TRY(send_command(enable_slot));

		const uint8_t slot_id = result.command_completion_event.slot_id;
		if (slot_id == 0 || slot_id > capability_regs().hcsparams1.max_slots)
		{
			dwarnln("EnableSlotCommand returned an invalid slot {}", slot_id);
			return BAN::Error::from_errno(EFAULT);
		}

#if DEBUG_XHCI
		const auto& root_port = m_ports[(route_string & 0x0F) - 1];

		dprintln("Initializing USB {H}.{H} device on slot {}",
			root_port.revision_major,
			root_port.revision_minor,
			slot_id
		);
#endif

		const XHCIDevice::Info info {
			.parent_hub = parent_hub,
			.parent_port_id = parent_port_id,
			.speed_class = speed_class,
			.slot_id = slot_id,
			.route_string = route_string,
			.depth = depth
		};

		m_slots[slot_id - 1] = TRY(XHCIDevice::create(*this, info));
		if (auto ret = m_slots[slot_id - 1]->initialize(); ret.is_error())
		{
			dwarnln("Could not initialize device on slot {}: {}", slot_id, ret.error());
			m_slots[slot_id - 1].clear();
			return ret.release_error();
		}

#if DEBUG_XHCI
		dprintln("USB {H}.{H} device on slot {} initialized",
			root_port.revision_major,
			root_port.revision_minor,
			slot_id
		);
#endif

		return slot_id;
	}

	void XHCIController::deinitialize_slot(uint8_t slot_id)
	{
		ASSERT(0 < slot_id && slot_id <= m_slots.size());
		ASSERT(m_slots[slot_id - 1]);
		m_slots[slot_id - 1]->destroy();
		m_slots[slot_id - 1].clear();
	}

	BAN::ErrorOr<void> XHCIController::reset_controller()
	{
		auto& operational = operational_regs();

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 500;

		// wait until controller not ready clears
		while (operational.usbsts & XHCI::USBSTS::ControllerNotReady)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		// issue software reset and wait for it to clear
		operational.usbcmd.host_controller_reset = 1;
		while (operational.usbcmd.host_controller_reset)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		return {};
	}

	BAN::ErrorOr<XHCI::TRB> XHCIController::send_command(const XHCI::TRB& trb)
	{
		LockGuard _(m_mutex);

		auto& operational = operational_regs();
		if (operational.usbsts & XHCI::USBSTS::HCHalted)
		{
			dwarnln("Trying to send a command on a halted controller");
			return BAN::Error::from_errno(EFAULT);
		}

		auto& command_trb = reinterpret_cast<volatile XHCI::TRB*>(m_command_ring_region->vaddr())[m_command_enqueue];
		command_trb.raw.dword0 = trb.raw.dword0;
		command_trb.raw.dword1 = trb.raw.dword1;
		command_trb.raw.dword2 = trb.raw.dword2;
		command_trb.raw.dword3 = trb.raw.dword3;
		command_trb.cycle = m_command_cycle;

		auto& completion_trb = const_cast<volatile XHCI::TRB&>(m_command_completions[m_command_enqueue]);
		completion_trb.raw.dword0 = 0;
		completion_trb.raw.dword1 = 0;
		completion_trb.raw.dword2 = 0;
		completion_trb.raw.dword3 = 0;

		advance_command_enqueue();

		doorbell_reg(0) = 0;

		uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 1000;
		while ((__atomic_load_n(&completion_trb.raw.dword2, __ATOMIC_SEQ_CST) >> 24) == 0)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		if (completion_trb.command_completion_event.completion_code != 1)
		{
			dwarnln("Completion error: {}", +completion_trb.command_completion_event.completion_code);
			return BAN::Error::from_errno(EFAULT);
		}

		return BAN::bit_cast<XHCI::TRB>(completion_trb);
	}

	void XHCIController::advance_command_enqueue()
	{
		m_command_enqueue++;
		if (m_command_enqueue < m_command_ring_trb_count - 1)
			return;

		auto& link_trb = reinterpret_cast<volatile XHCI::TRB*>(m_command_ring_region->vaddr())[m_command_enqueue].link_trb;
		link_trb.trb_type                = XHCI::TRBType::Link;
		link_trb.ring_segment_ponter     = m_command_ring_region->paddr();
		link_trb.interrupter_target      = 0;
		link_trb.cycle_bit               = m_command_cycle;
		link_trb.toggle_cycle            = 1;
		link_trb.chain_bit               = 0;
		link_trb.interrupt_on_completion = 0;

		m_command_enqueue = 0;
		m_command_cycle = !m_command_cycle;
	}

	void XHCIController::handle_irq()
	{
		auto& primary_interrupter = runtime_regs().irs[0];
		if (m_pci_device.interrupt_mechanism() != PCI::Device::InterruptMechanism::MSI && m_pci_device.interrupt_mechanism() != PCI::Device::InterruptMechanism::MSIX)
			primary_interrupter.iman = primary_interrupter.iman | XHCI::IMAN::InterruptPending | XHCI::IMAN::InterruptEnable;

		auto& operational = operational_regs();
		if (!(operational.usbsts & XHCI::USBSTS::EventInterrupt))
			return;
		operational.usbsts = XHCI::USBSTS::EventInterrupt;

		for (;;)
		{
			auto& trb = current_event_trb();
			if (trb.cycle != m_event_cycle)
				break;

			switch (trb.trb_type)
			{
				case XHCI::TRBType::TransferEvent:
				{
					dprintln_if(DEBUG_XHCI, "TransferEvent");

					const uint32_t slot_id = trb.transfer_event.slot_id;
					if (slot_id == 0 || slot_id > m_slots.size() || !m_slots[slot_id - 1])
					{
						dwarnln("TransferEvent for invalid slot {}", slot_id);
						dwarnln("Completion error: {}", +trb.transfer_event.completion_code);
						break;
					}

					m_slots[slot_id - 1]->on_transfer_event(trb);

					break;
				}
				case XHCI::TRBType::CommandCompletionEvent:
				{
					dprintln_if(DEBUG_XHCI, "CommandCompletionEvent");

					const uint32_t trb_index = (trb.command_completion_event.command_trb_pointer - m_command_ring_region->paddr()) / sizeof(XHCI::TRB);

					// NOTE: dword2 is last (and atomic) as that is what send_command is waiting for
					auto& completion_trb = const_cast<volatile XHCI::TRB&>(m_command_completions[trb_index]);
					completion_trb.raw.dword0 = trb.raw.dword0;
					completion_trb.raw.dword1 = trb.raw.dword1;
					completion_trb.raw.dword3 = trb.raw.dword3;
					__atomic_store_n(&completion_trb.raw.dword2, trb.raw.dword2, __ATOMIC_SEQ_CST);

					break;
				}
				case XHCI::TRBType::PortStatusChangeEvent:
				{
					dprintln_if(DEBUG_XHCI, "PortStatusChangeEvent");
					uint8_t port_id = trb.port_status_chage_event.port_id;
					if (port_id > capability_regs().hcsparams1.max_ports)
					{
						dwarnln("PortStatusChangeEvent on non-existent port {}", port_id);
						break;
					}
					m_port_changed = true;
					m_port_thread_blocker.unblock();
					break;
				}
				case XHCI::TRBType::BandwidthRequestEvent:
					dwarnln("Unhandled BandwidthRequestEvent");
					break;
				case XHCI::TRBType::DoorbellEvent:
					dwarnln("Unhandled DoorbellEvent");
					break;
				case XHCI::TRBType::HostControllerEvent:
					dwarnln("Unhandled HostControllerEvent");
					break;
				case XHCI::TRBType::DeviceNotificationEvent:
					dwarnln("Unhandled DeviceNotificationEvent");
					break;
				case XHCI::TRBType::MFINDEXWrapEvent:
					dwarnln("Unhandled MFINDEXWrapEvent");
					break;
				default:
					dwarnln("Unrecognized event TRB type {}", +trb.trb_type);
					break;
			}

			m_event_dequeue++;
			if (m_event_dequeue >= m_event_ring_trb_count)
			{
				m_event_dequeue = 0;
				m_event_cycle = !m_event_cycle;
			}
		}

		primary_interrupter.erdp = (m_event_ring_region->paddr() + (m_event_dequeue * sizeof(XHCI::TRB))) | XHCI::ERDP::EventHandlerBusy;
	}

	volatile XHCI::CapabilityRegs& XHCIController::capability_regs()
	{
		return *reinterpret_cast<volatile XHCI::CapabilityRegs*>(m_configuration_bar->vaddr());
	}

	volatile XHCI::OperationalRegs& XHCIController::operational_regs()
	{
		return *reinterpret_cast<volatile XHCI::OperationalRegs*>(m_configuration_bar->vaddr() + capability_regs().caplength);
	}

	volatile XHCI::RuntimeRegs& XHCIController::runtime_regs()
	{
		return *reinterpret_cast<volatile XHCI::RuntimeRegs*>(m_configuration_bar->vaddr() + (capability_regs().rstoff & ~0x1Fu));
	}

	volatile uint32_t& XHCIController::doorbell_reg(uint32_t slot_id)
	{
		return reinterpret_cast<volatile uint32_t*>(m_configuration_bar->vaddr() + capability_regs().dboff)[slot_id];
	}

	const volatile XHCI::TRB& XHCIController::current_event_trb()
	{
		return reinterpret_cast<const volatile XHCI::TRB*>(m_event_ring_region->vaddr())[m_event_dequeue];;
	}

	volatile uint64_t& XHCIController::dcbaa_reg(uint32_t slot_id)
	{
		return reinterpret_cast<volatile uint64_t*>(m_dcbaa_region->vaddr())[slot_id];
	}

}
