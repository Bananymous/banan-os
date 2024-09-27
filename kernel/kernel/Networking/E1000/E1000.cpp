#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Networking/E1000/E1000.h>
#include <kernel/Networking/NetworkManager.h>

namespace Kernel
{

	// https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf (section 5.2)
	bool E1000::probe(PCI::Device& pci_device)
	{
		// Intel device
		if (pci_device.vendor_id() != 0x8086)
			return false;

		switch (pci_device.device_id())
		{
			case 0x1019:
			case 0x101A:
			case 0x1010:
			case 0x1012:
			case 0x101D:
			case 0x1079:
			case 0x107A:
			case 0x107B:
			case 0x100F:
			case 0x1011:
			case 0x1026:
			case 0x1027:
			case 0x1028:
			case 0x1107:
			case 0x1112:
			case 0x1013:
			case 0x1018:
			case 0x1076:
			case 0x1077:
			case 0x1078:
			case 0x1017:
			case 0x1016:
			case 0x100e:
			case 0x1015:
				return true;
			default:
				return false;
		}
	}

	BAN::ErrorOr<BAN::RefPtr<E1000>> E1000::create(PCI::Device& pci_device)
	{
		auto e1000 = TRY(BAN::RefPtr<E1000>::create(pci_device));
		TRY(e1000->initialize());
		return e1000;
	}

	E1000::~E1000()
	{
	}

	BAN::ErrorOr<void> E1000::initialize()
	{
		m_bar_region = TRY(m_pci_device.allocate_bar_region(0));
		m_pci_device.enable_bus_mastering();

		detect_eeprom();
		TRY(read_mac_address());
		dprintln("E1000 at PCI {}:{}.{}", m_pci_device.bus(), m_pci_device.dev(), m_pci_device.func());
		dprintln("  MAC: {}", m_mac_address);

		TRY(initialize_rx());
		TRY(initialize_tx());

		enable_link();
		TRY(enable_interrupt());

		m_link_up = !!(read32(REG_STATUS) & STATUS_LU);

		dprintln("  link up: {}", link_up());
		if (link_up())
		{
			int speed = link_speed();
			dprintln("  link speed: {} Mbps", speed);
		}

		return {};
	}

	void E1000::write32(uint16_t reg, uint32_t value)
	{
		m_bar_region->write32(reg, value);
	}

	uint32_t E1000::read32(uint16_t reg)
	{
		return m_bar_region->read32(reg);
	}

	void E1000::detect_eeprom()
	{
		m_has_eerprom = false;
		write32(REG_EERD, 0x01);
		for (int i = 0; i < 1000 && !m_has_eerprom; i++)
			if (read32(REG_EERD) & 0x10)
				m_has_eerprom = true;
	}

	uint32_t E1000::eeprom_read(uint8_t address)
	{
		uint32_t tmp = 0;
		if (m_has_eerprom)
		{
			write32(REG_EERD, ((uint32_t)address << 8) | 1);
			while (!((tmp = read32(REG_EERD)) & (1 << 4)))
				continue;
		}
		else
		{
			write32(REG_EERD, ((uint32_t)address << 2) | 1);
			while (!((tmp = read32(REG_EERD)) & (1 << 1)))
				continue;
		}
		return (tmp >> 16) & 0xFFFF;
	}

	BAN::ErrorOr<void> E1000::read_mac_address()
	{
		if (m_has_eerprom)
		{
			uint32_t temp = eeprom_read(0);
			m_mac_address.address[0] = temp;
			m_mac_address.address[1] = temp >> 8;

			temp = eeprom_read(1);
			m_mac_address.address[2] = temp;
			m_mac_address.address[3] = temp >> 8;

			temp = eeprom_read(2);
			m_mac_address.address[4] = temp;
			m_mac_address.address[5] = temp >> 8;

			return {};
		}

		if (read32(0x5400) == 0)
		{
			dwarnln("no mac address");
			return BAN::Error::from_errno(EINVAL);
		}

		for (int i = 0; i < 6; i++)
			m_mac_address.address[i] = (uint8_t)read32(0x5400 + i * 8);

		return {};
	}

	BAN::ErrorOr<void> E1000::initialize_rx()
	{
		m_rx_buffer_region = TRY(DMARegion::create(E1000_RX_BUFFER_SIZE * E1000_RX_DESCRIPTOR_COUNT));
		m_rx_descriptor_region = TRY(DMARegion::create(sizeof(e1000_rx_desc) * E1000_RX_DESCRIPTOR_COUNT));

		auto* rx_descriptors = reinterpret_cast<volatile e1000_rx_desc*>(m_rx_descriptor_region->vaddr());
		for (size_t i = 0; i < E1000_RX_DESCRIPTOR_COUNT; i++)
		{
			rx_descriptors[i].addr = m_rx_buffer_region->paddr() + E1000_RX_BUFFER_SIZE * i;
			rx_descriptors[i].status = 0;
		}

		uint64_t paddr64 = m_rx_descriptor_region->paddr();
		write32(REG_RDBAL0, paddr64 & 0xFFFFFFFF);
		write32(REG_RDBAH0, paddr64 >> 32);
		write32(REG_RDLEN0, E1000_RX_DESCRIPTOR_COUNT * sizeof(e1000_rx_desc));
		write32(REG_RDH0, 0);
		write32(REG_RDT0, E1000_RX_DESCRIPTOR_COUNT - 1);

		uint32_t rctrl = 0;
		rctrl |= RCTL_EN;
		rctrl |= RCTL_SBP;
		rctrl |= RCTL_UPE;
		rctrl |= RCTL_MPE;
		rctrl |= RCTL_LBM_NORMAL;
		rctrl |= RCTL_RDMTS_1_2;
		rctrl |= RCTL_BAM;
		rctrl |= RCTL_SECRC;
		rctrl |= RCTL_BSIZE_8192;
   		write32(REG_RCTL, rctrl);

		return {};
	}

	BAN::ErrorOr<void> E1000::initialize_tx()
	{
		m_tx_buffer_region = TRY(DMARegion::create(E1000_TX_BUFFER_SIZE * E1000_TX_DESCRIPTOR_COUNT));
		m_tx_descriptor_region = TRY(DMARegion::create(sizeof(e1000_tx_desc) * E1000_TX_DESCRIPTOR_COUNT));

		auto* tx_descriptors = reinterpret_cast<volatile e1000_tx_desc*>(m_tx_descriptor_region->vaddr());
		for (size_t i = 0; i < E1000_TX_DESCRIPTOR_COUNT; i++)
		{
			tx_descriptors[i].addr = m_tx_buffer_region->paddr() + E1000_TX_BUFFER_SIZE * i;
			tx_descriptors[i].cmd = 0;
		}

		uint64_t paddr64 = m_tx_descriptor_region->paddr();
		write32(REG_TDBAL, paddr64 & 0xFFFFFFFF);
		write32(REG_TDBAH, paddr64 >> 32);
		write32(REG_TDLEN, E1000_TX_DESCRIPTOR_COUNT * sizeof(e1000_tx_desc));
		write32(REG_TDH, 0);
		write32(REG_TDT, 0);

		write32(REG_TCTL, TCTL_EN | TCTL_PSP);
		write32(REG_TIPG, 0x0060200A);

		return {};
	}

	void E1000::enable_link()
	{
		write32(REG_CTRL, read32(REG_CTRL) | CTRL_SLU);
	}

	int E1000::link_speed()
	{
		if (!link_up())
			return 0;
		uint32_t speed = read32(REG_STATUS) & STATUS_SPEED_MASK;
		if (speed == STATUS_SPEED_10MB)
			return 10;
		if (speed == STATUS_SPEED_100MB)
			return 100;
		if (speed == STATUS_SPEED_1000MB1)
			return 1000;
		if (speed == STATUS_SPEED_1000MB2)
			return 1000;
		return 0;
	}

	BAN::ErrorOr<void> E1000::enable_interrupt()
	{
		write32(REG_ITR, 0x1000);

		write32(REG_IVAR, 1 << 3);
		write32(REG_EITR, 0x1000);

		write32(REG_IMS, IMC_RxQ0);
		read32(REG_ICR);

		TRY(m_pci_device.reserve_interrupts(1));
		m_pci_device.enable_interrupt(0, *this);

		return {};
	}


	BAN::ErrorOr<void> E1000::send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() + sizeof(EthernetHeader) <= E1000_TX_BUFFER_SIZE);

		SpinLockGuard _(m_lock);

		size_t tx_current = read32(REG_TDT) % E1000_TX_DESCRIPTOR_COUNT;

		auto* tx_buffer = reinterpret_cast<uint8_t*>(m_tx_buffer_region->vaddr() + E1000_TX_BUFFER_SIZE * tx_current);

		auto& ethernet_header = *reinterpret_cast<EthernetHeader*>(tx_buffer);
		ethernet_header.dst_mac = destination;
		ethernet_header.src_mac = get_mac_address();
		ethernet_header.ether_type = protocol;

		memcpy(tx_buffer + sizeof(EthernetHeader), buffer.data(), buffer.size());

		auto& descriptor = reinterpret_cast<volatile e1000_tx_desc*>(m_tx_descriptor_region->vaddr())[tx_current];
		descriptor.length = sizeof(EthernetHeader) + buffer.size();
		descriptor.status = 0;
		descriptor.cmd = CMD_EOP | CMD_IFCS | CMD_RS;

		write32(REG_TDT, (tx_current + 1) % E1000_TX_DESCRIPTOR_COUNT);
		while (descriptor.status == 0)
			continue;

		dprintln_if(DEBUG_E1000, "sent {} bytes", sizeof(EthernetHeader) + buffer.size());

		return {};
	}

	void E1000::handle_irq()
	{
		if (!(read32(REG_ICR) & ICR_RxQ0))
			return;

		SpinLockGuard _(m_lock);

		for (;;) {
			uint32_t rx_current = (read32(REG_RDT0) + 1) % E1000_RX_DESCRIPTOR_COUNT;

			auto& descriptor = reinterpret_cast<volatile e1000_rx_desc*>(m_rx_descriptor_region->vaddr())[rx_current];
			if (!(descriptor.status & 1))
				break;
			ASSERT(descriptor.length <= E1000_RX_BUFFER_SIZE);

			dprintln_if(DEBUG_E1000, "got {} bytes", (uint16_t)descriptor.length);

			NetworkManager::get().on_receive(*this, BAN::ConstByteSpan {
				reinterpret_cast<const uint8_t*>(m_rx_buffer_region->vaddr() + rx_current * E1000_RX_BUFFER_SIZE),
				descriptor.length
			});

			descriptor.status = 0;
			write32(REG_RDT0, rx_current);
		}
	}

}
