#pragma once

#include <BAN/Vector.h>

#include <kernel/Lock/Mutex.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/ThreadBlocker.h>
#include <kernel/USB/Definitions.h>
#include <kernel/USB/USBManager.h>
#include <kernel/USB/XHCI/Definitions.h>

namespace Kernel
{

	class XHCIDevice;

	class XHCIController : public USBController, public Interruptable
	{
		BAN_NON_COPYABLE(XHCIController);
		BAN_NON_MOVABLE(XHCIController);

	public:
		struct Port
		{
			uint8_t revision_major { 0 };
			uint8_t revision_minor { 0 };
			uint8_t slot_type { 0 };
			uint8_t slot_id { 0 };
		};

	public:
		static BAN::ErrorOr<void> take_ownership(PCI::Device&);
		static BAN::ErrorOr<BAN::UniqPtr<XHCIController>> create(PCI::Device&);

		void handle_irq() final override;

	private:
		XHCIController(PCI::Device& pci_device);
		~XHCIController();

		BAN::ErrorOr<void> initialize_impl();
		BAN::ErrorOr<void> initialize_ports();
		BAN::ErrorOr<void> initialize_primary_interrupter();
		BAN::ErrorOr<void> initialize_scratchpad();

		BAN::ErrorOr<void> reset_controller();

		void port_updater_task();

		BAN::ErrorOr<void> initialize_slot(int port_index);

		BAN::ErrorOr<XHCI::TRB> send_command(const XHCI::TRB&);
		void advance_command_enqueue();

		bool context_size_set() { return capability_regs().hccparams1.context_size; }

		const Port& port(uint32_t port_id) const { return m_ports[port_id - 1]; }

		volatile XHCI::CapabilityRegs& capability_regs();
		volatile XHCI::OperationalRegs& operational_regs();
		volatile XHCI::RuntimeRegs& runtime_regs();
		volatile uint32_t& doorbell_reg(uint32_t slot_id);
		volatile uint64_t& dcbaa_reg(uint32_t slot_id);

		const volatile XHCI::TRB& current_event_trb();

		uint8_t speed_class_to_id(USB::SpeedClass speed_class) const;
		USB::SpeedClass speed_id_to_class(uint8_t) const;

	private:
		static constexpr uint32_t m_command_ring_trb_count = 256;
		static constexpr uint32_t m_event_ring_trb_count = 252;

		Mutex m_mutex;

		Process* m_port_updater { nullptr };
		ThreadBlocker m_port_thread_blocker;
		BAN::Atomic<bool> m_port_changed { false };

		PCI::Device& m_pci_device;
		BAN::UniqPtr<PCI::BarRegion> m_configuration_bar;
		BAN::UniqPtr<DMARegion> m_dcbaa_region;

		BAN::UniqPtr<DMARegion> m_command_ring_region;
		uint32_t m_command_enqueue { 0 };
		bool m_command_cycle { 1 };

		BAN::UniqPtr<DMARegion> m_scratchpad_buffer_array;
		BAN::Vector<paddr_t> m_scratchpad_buffers;

		BAN::UniqPtr<DMARegion> m_event_ring_region;
		uint32_t m_event_dequeue { 0 };
		bool m_event_cycle { 1 };

		BAN::Vector<XHCI::TRB> m_command_completions;

		BAN::Vector<Port>						m_ports;
		BAN::Vector<BAN::UniqPtr<XHCIDevice>>	m_slots;

		friend class XHCIDevice;
		friend class BAN::UniqPtr<XHCIController>;
	};

}
