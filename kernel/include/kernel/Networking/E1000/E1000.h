#pragma once

#include <BAN/UniqPtr.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/Networking/E1000/Definitions.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/PCI.h>

#define E1000_RX_DESCRIPTOR_COUNT 256
#define E1000_TX_DESCRIPTOR_COUNT 256

#define E1000_RX_BUFFER_SIZE 8192
#define E1000_TX_BUFFER_SIZE 8192

namespace Kernel
{

	class E1000 : public NetworkInterface, public Interruptable
	{
	public:
		static bool probe(PCI::Device&);
		static BAN::ErrorOr<BAN::RefPtr<E1000>> create(PCI::Device&);
		~E1000();

		virtual BAN::MACAddress get_mac_address() const override { return m_mac_address; }

		virtual bool link_up() override { return m_link_up; }
		virtual int link_speed() override;

		virtual size_t payload_mtu() const { return E1000_RX_BUFFER_SIZE - sizeof(EthernetHeader); }

		virtual void handle_irq() final override;

	protected:
		E1000(PCI::Device& pci_device)
			: m_pci_device(pci_device)
		{ }
		BAN::ErrorOr<void> initialize();

		virtual void detect_eeprom();
		virtual uint32_t eeprom_read(uint8_t addr);

		uint32_t read32(uint16_t reg);
		void write32(uint16_t reg, uint32_t value);

		virtual BAN::ErrorOr<void> send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan) override;

	private:
		BAN::ErrorOr<void> read_mac_address();

		BAN::ErrorOr<void> initialize_rx();
		BAN::ErrorOr<void> initialize_tx();

		void enable_link();
		BAN::ErrorOr<void> enable_interrupt();

		void handle_receive();

	protected:
		PCI::Device&					m_pci_device;
		BAN::UniqPtr<PCI::BarRegion>	m_bar_region;
		bool							m_has_eerprom { false };

	private:
		BAN::UniqPtr<DMARegion>	m_rx_buffer_region;
		BAN::UniqPtr<DMARegion>	m_tx_buffer_region;
		BAN::UniqPtr<DMARegion>	m_rx_descriptor_region;
		BAN::UniqPtr<DMARegion>	m_tx_descriptor_region;

		BAN::MACAddress	m_mac_address {};
		bool			m_link_up { false };

		friend class BAN::RefPtr<E1000>;
	};

}
