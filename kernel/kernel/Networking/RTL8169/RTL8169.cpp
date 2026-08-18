#include <kernel/Lock/BlockableSpinLock.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/RTL8169/Definitions.h>
#include <kernel/Networking/RTL8169/RTL8169.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	// each buffer is 7440 bytes + padding = 8192
	constexpr size_t s_buffer_size = 8192;

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

		// disable rx, tx, checksum offload
		m_io_bar_region->write16(RTL8169_IO_CPlusCR, 0x0000);
		m_io_bar_region->write8 (RTL8169_IO_CR,      0x00);

		dprintln("  reset done");

		for (size_t i = 0; i < 6; i++)
			m_mac_address.address[i] = m_io_bar_region->read8(RTL8169_IO_IDR0 + i);
		dprintln("  MAC {}", m_mac_address);

		TRY(initialize_rx());
		TRY(initialize_tx());
		m_io_bar_region->write8(RTL8169_IO_CR, RTL8169_CR_RE | RTL8169_CR_TE);
		dprintln("  descriptors initialized");

		m_link_up = m_io_bar_region->read8(RTL8169_IO_PHYSts) & RTL8169_PHYSts_LinkSts;
		dprintln("  link status {}", link_up() ? "UP" : "DOWN");
		if (link_up())
			dprintln("  link speed {}", link_speed());

		TRY(enable_interrupt());
		dprintln("  interrupts enabled");

		auto* thread = TRY(Thread::create_kernel([](void* rtl8169_ptr) {
			static_cast<RTL8169*>(rtl8169_ptr)->receive_thread();
		}, this));
		if (auto ret = Processor::scheduler().add_thread(thread); ret.is_error())
		{
			delete thread;
			return ret.release_error();
		}
		m_rx_thread_is_dead = false;

		return {};
	}

	RTL8169::~RTL8169()
	{
		m_rx_thread_should_die = true;
		m_rx_blocker.unblock();

		while (!m_rx_thread_is_dead)
			Processor::yield();
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
		m_rx_buffer_region = TRY(DMARegion::create(m_rx_descriptor_count * s_buffer_size, PageTable::MemoryType::Normal));
		m_rx_descriptor_region = TRY(DMARegion::create(m_rx_descriptor_count * sizeof(RTL8169Descriptor)));

		auto* rx_descriptors = reinterpret_cast<volatile RTL8169Descriptor*>(m_rx_descriptor_region->vaddr());
		for (size_t i = 0; i < m_rx_descriptor_count; i++)
		{
			const paddr_t rx_buffer_paddr = m_rx_buffer_region->paddr() + i * s_buffer_size;
			rx_descriptors[i].command     = 0x1FF8 | RTL8169_DESC_CMD_OWN;
			rx_descriptors[i].vlan        = 0;
			rx_descriptors[i].buffer_low  = rx_buffer_paddr & 0xFFFFFFFF;
			rx_descriptors[i].buffer_high = rx_buffer_paddr >> 32;
		}
		rx_descriptors[m_rx_descriptor_count - 1].command |= RTL8169_DESC_CMD_EOR;

		// configure rx descriptor addresses
		m_io_bar_region->write32(RTL8169_IO_RDSAR + 4, m_rx_descriptor_region->paddr() >> 32);
		m_io_bar_region->write32(RTL8169_IO_RDSAR + 0, m_rx_descriptor_region->paddr() & 0xFFFFFFFF);

		// configure receive control (no fifo threshold, max dma burst unlimited, broadcast, multicast, accept physical match)
		m_io_bar_region->write32(RTL8169_IO_RCR,
			RTL8169_RCR_RXFTH_NO | RTL8169_RCR_MXDMA_UNLIMITED | RTL8169_RCR_AB | RTL8169_RCR_AM | RTL8169_RCR_APM
		);

		// configure max rx packet size
		m_io_bar_region->write16(RTL8169_IO_RMS, RTL8169_RMS_MAX);

		// enable ip/tcp/udp checksum offloading
		// TODO: is this supported on all cards?
		m_io_bar_region->write16(RTL8169_IO_CPlusCR, 1 << 5);

		return {};
	}

	BAN::ErrorOr<void> RTL8169::initialize_tx()
	{
		m_tx_buffer_region = TRY(DMARegion::create(m_tx_descriptor_count * s_buffer_size, PageTable::MemoryType::Normal));
		m_tx_descriptor_region = TRY(DMARegion::create(m_tx_descriptor_count * sizeof(RTL8169Descriptor)));

		auto* tx_descriptors = reinterpret_cast<volatile RTL8169Descriptor*>(m_tx_descriptor_region->vaddr());
		for (size_t i = 0; i < m_tx_descriptor_count; i++)
		{
			const paddr_t tx_buffer_paddr = m_tx_buffer_region->paddr() + i * s_buffer_size;
			tx_descriptors[i].command     = 0;
			tx_descriptors[i].vlan        = 0;
			tx_descriptors[i].buffer_low  = tx_buffer_paddr & 0xFFFFFFFF;
			tx_descriptors[i].buffer_high = tx_buffer_paddr >> 32;
		}
		tx_descriptors[m_tx_descriptor_count - 1].command |= RTL8169_DESC_CMD_EOR;

		// configure tx descriptor addresses
		m_io_bar_region->write32(RTL8169_IO_TNPDS + 4, m_tx_descriptor_region->paddr() >> 32);
		m_io_bar_region->write32(RTL8169_IO_TNPDS + 0, m_tx_descriptor_region->paddr() & 0xFFFFFFFF);

		// configure transmit control (standard ifg, max dma burst unlimited)
		m_io_bar_region->write32(RTL8169_IO_TCR, RTL8169_TCR_IFG_0 | RTL8169_TCR_MXDMA_UNLIMITED);

		// configure max tx packet size
		m_io_bar_region->write8(RTL8169_IO_MTPS, RTL8169_MTPS_MAX);

		return {};
	}

	BAN::ErrorOr<void> RTL8169::enable_interrupt()
	{
		TRY(m_pci_device.reserve_interrupts(1));
		m_pci_device.enable_interrupt(0, *this);

		m_io_bar_region->write16(RTL8169_IO_IMR,
			RTL8169_IR_ROK |
			RTL8169_IR_RER |
			RTL8169_IR_TOK |
			RTL8169_IR_TER |
			RTL8169_IR_RDU |
			RTL8169_IR_LinkChg |
			RTL8169_IR_FVOW
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

	BAN::ErrorOr<void> RTL8169::send_raw_bytes(BAN::Span<const BAN::ConstByteSpan> buffers)
	{
		if (!link_up())
			return BAN::Error::from_errno(EADDRNOTAVAIL);

		const auto interrupt_state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		const uint32_t tx_current_nowrap = m_tx_head.fetch_add(1);
		const uint32_t tx_current = tx_current_nowrap % m_tx_descriptor_count;

		auto& descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_tx_descriptor_region->vaddr())[tx_current];
		if (descriptor.command & RTL8169_DESC_CMD_OWN)
		{
			SpinLockGuard guard(m_tx_lock);
			while (descriptor.command & RTL8169_DESC_CMD_OWN)
			{
				BlockableSpinLock block(m_tx_lock);
				m_tx_blocker.block_indefinite(&block);
			}
		}

		auto* tx_buffer = reinterpret_cast<uint8_t*>(m_tx_buffer_region->vaddr() + tx_current * s_buffer_size);

		// write packet
		size_t packet_size = 0;
		for (const auto& buffer : buffers)
		{
			ASSERT(packet_size + buffer.size() <= s_buffer_size);
			memcpy(tx_buffer + packet_size, buffer.data(), buffer.size());
			packet_size += buffer.size();
		}

		// give packet ownership to NIC
		uint32_t command = packet_size | RTL8169_DESC_CMD_OWN | RTL8169_DESC_CMD_LS | RTL8169_DESC_CMD_FS;
		if (tx_current == m_tx_descriptor_count - 1)
			command |= RTL8169_DESC_CMD_EOR;
		descriptor.command = command;

		// ring tx queue doorbell
		if (tx_current_nowrap == m_tx_commit.load())
			m_io_bar_region->write8(RTL8169_IO_TPPoll, RTL8169_TPPoll_NPQ);
		while (tx_current_nowrap != m_tx_commit.load())
			Processor::pause();
		m_tx_commit.add_fetch(1);

		Processor::set_interrupt_state(interrupt_state);

		return {};
	}

	void RTL8169::receive_thread()
	{
		SpinLockGuard rx_lock_guard(m_rx_lock);

		while (!m_rx_thread_should_die)
		{
			for (;;)
			{
				auto& descriptor = reinterpret_cast<volatile RTL8169Descriptor*>(m_rx_descriptor_region->vaddr())[m_rx_head];

				const auto command = descriptor.command;
				if (descriptor.command & RTL8169_DESC_CMD_OWN)
					break;

				// packet buffer can only hold single packet, so we should not receive any multi-descriptor packets
				ASSERT((command & RTL8169_DESC_CMD_LS) && (command & RTL8169_DESC_CMD_FS));

				const uint8_t protocol = (command >> 17) & 3;
				const uint16_t packet_length = command & 0x3FFF;
				if (packet_length > s_buffer_size)
					dwarnln("Got {} bytes to {} byte buffer", packet_length, s_buffer_size);
				else if (command & (1u << 21))
					dwarnln("descriptor error {4h}", command);
				else if (protocol == 1 && (command & (1u << 14)))
					dwarnln("TCP checksum error");
				else if (protocol == 2 && (command & (1u << 15)))
					dwarnln("UDP checksum error");
				else if (protocol != 0 && (command & (1u << 16)))
					dwarnln("IPv4 checksum error");
				else
				{
					m_rx_lock.unlock(InterruptState::Enabled);

					uint32_t validated_cksums;
					switch (protocol)
					{
						case 0: validated_cksums = 0;                      break;
						case 1: validated_cksums = CKSUM_IPV4 | CKSUM_TCP; break;
						case 2: validated_cksums = CKSUM_IPV4 | CKSUM_UDP; break;
						case 3: validated_cksums = CKSUM_IPV4;             break;
					}

					const uint8_t* packet_data = reinterpret_cast<const uint8_t*>(m_rx_buffer_region->vaddr() + m_rx_head * s_buffer_size);
					NetworkManager::get().on_receive(*this, BAN::ConstByteSpan { packet_data, packet_length }, validated_cksums);

					m_rx_lock.lock();
				}

				uint32_t new_command = 0x1FF8 | RTL8169_DESC_CMD_OWN;
				if (m_rx_head == m_rx_descriptor_count - 1)
					new_command |= RTL8169_DESC_CMD_EOR;
				descriptor.command = new_command;

				m_rx_head = (m_rx_head + 1) % m_rx_descriptor_count;
			}

			BlockableSpinLock block(m_rx_lock);
			m_rx_blocker.block_indefinite(&block);
		}

		m_rx_thread_is_dead = true;
	}

	void RTL8169::handle_irq()
	{
		uint16_t isr;
		while ((isr = m_io_bar_region->read16(RTL8169_IO_ISR)))
		{
			m_io_bar_region->write16(RTL8169_IO_ISR, isr);

			if (isr & RTL8169_IR_LinkChg)
			{
				m_link_up = m_io_bar_region->read8(RTL8169_IO_PHYSts) & RTL8169_PHYSts_LinkSts;
				dprintln("link status {}", link_up() ? "UP" : "DOWN");
				if (link_up())
					dprintln("link speed {}", link_speed());
			}

			if (isr & (RTL8169_IR_TER | RTL8169_IR_TOK))
			{
				SpinLockGuard _(m_tx_lock);
				m_tx_blocker.unblock();
			}

			if (isr & (RTL8169_IR_RER | RTL8169_IR_ROK))
			{
				SpinLockGuard _(m_rx_lock);
				m_rx_blocker.unblock();
			}

			if (isr & RTL8169_IR_RER)
				dwarnln("Rx error");
			if (isr & RTL8169_IR_TER)
				dwarnln("Tx error");
			if (isr & RTL8169_IR_RDU)
				dwarnln("Rx descriptor not available");
			if (isr & RTL8169_IR_FVOW)
				dwarnln("Rx FIFO overflow");
		}
	}

}
