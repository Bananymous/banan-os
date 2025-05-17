#pragma once

#include <BAN/UniqPtr.h>
#include <kernel/Interruptable.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/PCI.h>

namespace Kernel
{

	class RTL8169 final : public NetworkInterface, public Interruptable
	{
	public:
		static bool probe(PCI::Device&);
		static BAN::ErrorOr<BAN::RefPtr<RTL8169>> create(PCI::Device&);

		virtual BAN::MACAddress get_mac_address() const override { return m_mac_address; }

		virtual bool link_up() override { return m_link_up; }
		virtual int link_speed() override;

		virtual size_t payload_mtu() const override { return 7436 - sizeof(EthernetHeader); }

		virtual void handle_irq() override;

	protected:
		RTL8169(PCI::Device& pci_device)
			: NetworkInterface(Type::Ethernet)
			, m_pci_device(pci_device)
		{ }
		BAN::ErrorOr<void> initialize();

		virtual BAN::ErrorOr<void> send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan) override;

		virtual bool can_read_impl() const override { return false; }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override { return false; }

	private:
		BAN::ErrorOr<void> reset();

		BAN::ErrorOr<void> initialize_rx();
		BAN::ErrorOr<void> initialize_tx();

		void enable_link();
		BAN::ErrorOr<void> enable_interrupt();

		void handle_receive();

	protected:
		PCI::Device&					m_pci_device;
		BAN::UniqPtr<PCI::BarRegion>	m_io_bar_region;

	private:
		static constexpr size_t m_rx_descriptor_count = 256;
		static constexpr size_t m_tx_descriptor_count = 256;

		BAN::UniqPtr<DMARegion>	m_rx_buffer_region;
		BAN::UniqPtr<DMARegion>	m_tx_buffer_region;
		BAN::UniqPtr<DMARegion>	m_rx_descriptor_region;
		BAN::UniqPtr<DMARegion>	m_tx_descriptor_region;

		SpinLock m_lock;
		ThreadBlocker m_thread_blocker;

		uint32_t m_rx_current { 0 };
		size_t m_tx_current { 0 };

		BAN::MACAddress	m_mac_address {};
		BAN::Atomic<bool> m_link_up { false };

		friend class BAN::RefPtr<RTL8169>;
	};

}
