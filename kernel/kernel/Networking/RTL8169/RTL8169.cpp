#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/RTL8169/Definitions.h>
#include <kernel/Networking/RTL8169/RTL8169.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	bool RTL8169::probe(PCI::Device& pci_device)
	{
		if (pci_device.vendor_id() != 0x10ec)
			return false;
		switch (pci_device.device_id())
		{
			case 0x8161:
			case 0x8168:
			case 0x8169:
				return true;
			default:
				return false;
		}
	}

	BAN::ErrorOr<BAN::RefPtr<RTL8169>> RTL8169::create(PCI::Device& pci_device)
	{
		auto rtl8169 = TRY(BAN::RefPtr<RTL8169>::create(pci_device));
		TRY(rtl8169->initialize());
		return rtl8169;
	}

	BAN::ErrorOr<void> RTL8169::initialize()
	{
		m_pci_device.enable_bus_mastering();

		m_io_bar_region = TRY(m_pci_device.allocate_bar_region(0));
		if (m_io_bar_region->type() != PCI::BarType::IO)
		{
			dwarnln("RTL8169 BAR0 is not IO space");
			return BAN::Error::from_errno(EINVAL);
		}

		dprintln("Initializing RTL8169");

		TRY(reset());

		dprintln("  reset done");

		for (size_t i = 0; i < 6; i++)
			m_mac_address.address[i] = m_io_bar_region->read8(RTL8169_IO_IDR0 + i);
		dprintln("  MAC {}", m_mac_address);

		// unlock config registers
		m_io_bar_region->write8(RTL8169_IO_9346CR, RTL8169_9346CR_MODE_CONFIG);

		TRY(initialize_rx());
		TRY(initialize_tx());
		m_io_bar_region->write8(RTL8169_IO_CR, RTL8169_CR_RE | RTL8169_CR_TE);
		dprintln("  descriptors initialized");

		m_link_up = m_io_bar_region->read8(RTL8169_IO_PHYSts) & RTL8169_PHYSts_LinkSts;
		if (m_link_up)
			dprintln("  link speed {}", link_speed());

		TRY(enable_interrupt());
		dprintln("  interrupts enabled");

		// lock config registers
		m_io_bar_region->write8(RTL8169_IO_9346CR, RTL8169_9346CR_MODE_NORMAL);

		return {};
	}

	BAN::ErrorOr<void> RTL8169::reset()
	{
		m_io_bar_region->write8(RTL8169_IO_CR, RTL8169_CR_RST);

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 100;
		while (m_io_bar_region->read8(RTL8169_IO_CR) & RTL8169_CR_RST)
			if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

		return {};
	}

	BAN::ErrorOr<void> RTL8169::initialize_rx()
	{
		// each buffer is 7440 bytes + padding = 8192
		constexpr size_t buffer_size = 2 * PAGE_SIZE;

		m_rx_buffer_region = TRY(DMARegion::create(m_rx_descriptor_count * buffer_size));
		m_rx_descriptor_region = TRY(DMARegion::create(m_rx_descriptor_count * sizeof(RTL8169Descriptor)));

		for (size_t i = 0; i < m_rx_descriptor_count; i++)
		{
			const paddr_t rx_buffer_paddr = m_rx_buffer_region->paddr() + i * buffer_size;

			uint32_t command = 0x1FF8 | RTL8169_DESC_CMD_OWN;
			if (i == m_rx_descriptor_count - 1)
				command |= RTL8169_DESC_CMD_EOR;

			auto& rx_descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_rx_descriptor_region->vaddr())[i];
			rx_descriptor.command     = command;
			rx_descriptor.vlan        = 0;
			rx_descriptor.buffer_low  = rx_buffer_paddr & 0xFFFFFFFF;
			rx_descriptor.buffer_high = rx_buffer_paddr >> 32;
		}

		// configure rx descriptor addresses
		m_io_bar_region->write32(RTL8169_IO_RDSAR + 0, m_rx_descriptor_region->paddr() & 0xFFFFFFFF);
		m_io_bar_region->write32(RTL8169_IO_RDSAR + 4, m_rx_descriptor_region->paddr() >> 32);

		// configure receibe control (no fifo threshold, max dma burst 1024, accept physical match, broadcast, multicast)
		m_io_bar_region->write32(RTL8169_IO_RCR,
			RTL8169_RCR_RXFTH_NO | RTL8169_RCR_MXDMA_1024 | RTL8169_RCR_AB | RTL8169_RCR_AM | RTL8169_RCR_APM
		);

		m_io_bar_region->write32(0x44, 0b111111 | (0b111 << 8) | (0b111 << 13));

		// configure max rx packet size
		m_io_bar_region->write16(RTL8169_IO_RMS, RTL8169_RMS_MAX);


		return {};
	}

	BAN::ErrorOr<void> RTL8169::initialize_tx()
	{
		// each buffer is 7440 bytes + padding = 8192
		constexpr size_t buffer_size = 2 * PAGE_SIZE;

		m_tx_buffer_region = TRY(DMARegion::create(m_tx_descriptor_count * buffer_size));
		m_tx_descriptor_region = TRY(DMARegion::create(m_tx_descriptor_count * sizeof(RTL8169Descriptor)));

		for (size_t i = 0; i < m_tx_descriptor_count; i++)
		{
			const paddr_t tx_buffer_paddr = m_tx_buffer_region->paddr() + i * buffer_size;

			uint32_t command = 0;
			if (i == m_tx_descriptor_count - 1)
				command |= RTL8169_DESC_CMD_EOR;

			auto& tx_descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_tx_descriptor_region->vaddr())[i];
			tx_descriptor.command     = command;
			tx_descriptor.vlan        = 0;
			tx_descriptor.buffer_low  = tx_buffer_paddr & 0xFFFFFFFF;
			tx_descriptor.buffer_high = tx_buffer_paddr >> 32;
		}

		// configure tx descriptor addresses
		m_io_bar_region->write32(RTL8169_IO_TNPDS + 0, m_tx_descriptor_region->paddr() & 0xFFFFFFFF);
		m_io_bar_region->write32(RTL8169_IO_TNPDS + 4, m_tx_descriptor_region->paddr() >> 32);

		// configure transmit control (standard ifg, max dma burst 1024)
		m_io_bar_region->write32(RTL8169_IO_TCR, RTL8169_TCR_IFG_0 | RTL8169_TCR_MXDMA_1024);

		// configure max tx packet size
		m_io_bar_region->write8(RTL8169_IO_MTPS, RTL8169_MTPS_MAX);

		return {};
	}

	BAN::ErrorOr<void> RTL8169::enable_interrupt()
	{
		TRY(m_pci_device.reserve_interrupts(1));
		m_pci_device.enable_interrupt(0, *this);

		m_io_bar_region->write16(RTL8169_IO_IMR,
			RTL8169_IR_ROK
			| RTL8169_IR_RER
			| RTL8169_IR_TOK
			| RTL8169_IR_TER
			| RTL8169_IR_RDU
			| RTL8169_IR_LinkChg
			| RTL8169_IR_FVOW
			| RTL8169_IR_TDU
		);
		m_io_bar_region->write16(RTL8169_IO_ISR, 0xFFFF);

		return {};
	}

	int RTL8169::link_speed()
	{
		if (!link_up())
			return 0;
		const uint8_t phy_status = m_io_bar_region->read8(RTL8169_IO_PHYSts);
		if (phy_status & RTL8169_PHYSts_1000MF)
			return 1000;
		if (phy_status & RTL8169_PHYSts_100M)
			return 100;
		if (phy_status & RTL8169_PHYSts_10M)
			return 10;
		return 0;
	}

	BAN::ErrorOr<void> RTL8169::send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan buffer)
	{
		constexpr size_t buffer_size = 8192;

		const uint16_t packet_size = sizeof(EthernetHeader) + buffer.size();
		if (packet_size > buffer_size)
			return BAN::Error::from_errno(EINVAL);

		if (!link_up())
			return BAN::Error::from_errno(EADDRNOTAVAIL);

		auto state = m_lock.lock();
		const uint32_t tx_current = m_tx_current;
		m_tx_current = (m_tx_current + 1) % m_tx_descriptor_count;
		m_lock.unlock(state);

		auto& descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_tx_descriptor_region->vaddr())[tx_current];
		while (descriptor.command & RTL8169_DESC_CMD_OWN)
			m_thread_blocker.block_with_timeout_ms(100);

		auto* tx_buffer = reinterpret_cast<uint8_t*>(m_tx_buffer_region->vaddr() + tx_current * buffer_size);

		// write packet
		auto& ethernet_header = *reinterpret_cast<EthernetHeader*>(tx_buffer);
		ethernet_header.dst_mac = destination;
		ethernet_header.src_mac = get_mac_address();
		ethernet_header.ether_type = protocol;
		memcpy(tx_buffer + sizeof(EthernetHeader), buffer.data(), buffer.size());

		// give packet ownership to NIC
		uint32_t command = packet_size | RTL8169_DESC_CMD_OWN | RTL8169_DESC_CMD_LS | RTL8169_DESC_CMD_FS;
		if (tx_current >= m_tx_descriptor_count - 1)
			command |= RTL8169_DESC_CMD_EOR;
		descriptor.command = command;

		// notify NIC about new packet
		m_io_bar_region->write8(RTL8169_IO_TPPoll, RTL8169_TPPoll_NPQ);

		return {};
	}

	void RTL8169::handle_irq()
	{
		const uint16_t interrupt_status = m_io_bar_region->read16(RTL8169_IO_ISR);
		m_io_bar_region->write16(RTL8169_IO_ISR, interrupt_status);

		if (interrupt_status & RTL8169_IR_LinkChg)
		{
			m_link_up = m_io_bar_region->read8(RTL8169_IO_PHYSts) & RTL8169_PHYSts_LinkSts;
			dprintln("link status -> {}", m_link_up.load());
		}

		if (interrupt_status & RTL8169_IR_TOK)
			m_thread_blocker.unblock();

		if (interrupt_status & RTL8169_IR_RER)
			dwarnln("Rx error");
		if (interrupt_status & RTL8169_IR_TER)
			dwarnln("Tx error");
		if (interrupt_status & RTL8169_IR_RDU)
			dwarnln("Rx descriptor not available");
		if (interrupt_status & RTL8169_IR_FVOW)
			dwarnln("Rx FIFO overflow");
		// dont log TDU is sent after each sent packet

		if (!(interrupt_status & RTL8169_IR_ROK))
			return;

		constexpr size_t buffer_size = 8192;

		for (;;)
		{
			auto& descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_rx_descriptor_region->vaddr())[m_rx_current];
			if (descriptor.command & RTL8169_DESC_CMD_OWN)
				break;

			// packet buffer can only hold single packet, so we should not receive any multi-descriptor packets
			ASSERT((descriptor.command & RTL8169_DESC_CMD_LS) && (descriptor.command & RTL8169_DESC_CMD_FS));

			const uint16_t packet_length = descriptor.command & 0x3FFF;
			if (packet_length > buffer_size)
				dwarnln("Got {} bytes to {} byte buffer", packet_length, buffer_size);
			else if (descriptor.command & (1u << 21))
				; // descriptor has an error
			else
			{
				NetworkManager::get().on_receive(*this, BAN::ConstByteSpan {
					reinterpret_cast<const uint8_t*>(m_rx_buffer_region->vaddr() + m_rx_current * buffer_size),
					packet_length
				});
			}

			m_rx_current = (m_rx_current + 1) % m_rx_descriptor_count;

			descriptor.command = descriptor.command | RTL8169_DESC_CMD_OWN;
		}
	}

}
