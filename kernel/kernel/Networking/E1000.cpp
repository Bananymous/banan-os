#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Networking/E1000.h>

#define DEBUG_E1000 1

#define E1000_REG_CTRL				0x0000
#define E1000_REG_STATUS			0x0008
#define E1000_REG_EEPROM			0x0014
#define E1000_REG_INT_CAUSE_READ	0x00C0
#define E1000_REG_INT_RATE			0x00C4
#define E1000_REG_INT_MASK_SET		0x00D0
#define E1000_REG_INT_MASK_CLEAR	0x00D8
#define E1000_REG_RCTRL				0x0100
#define E1000_REG_RXDESCLO			0x2800
#define E1000_REG_RXDESCHI			0x2804
#define E1000_REG_RXDESCLEN			0x2808
#define E1000_REG_RXDESCHEAD		0x2810
#define E1000_REG_RXDESCTAIL		0x2818
#define E1000_REG_TXDESCLO			0x3800
#define E1000_REG_TXDESCHI			0x3804
#define E1000_REG_TXDESCLEN			0x3808
#define E1000_REG_TXDESCHEAD		0x3810
#define E1000_REG_TXDESCTAIL		0x3818
#define E1000_REG_TCTRL				0x0400
#define E1000_REG_TIPG				0x0410

#define E1000_STATUS_LINK_UP		0x02
#define E1000_STATUS_SPEED_MASK		0xC0
#define E1000_STATUS_SPEED_10MB		0x00
#define E1000_STATUS_SPEED_100MB	0x40
#define E1000_STATUS_SPEED_1000MB1	0x80
#define E1000_STATUS_SPEED_1000MB2	0xC0

#define E1000_CTRL_SET_LINK_UP		0x40

#define E1000_INT_TXDW		(1 << 0)
#define E1000_INT_TXQE		(1 << 1)
#define E1000_INT_LSC		(1 << 2)
#define E1000_INT_RXSEQ		(1 << 3)
#define E1000_INT_RXDMT0	(1 << 4)
#define E1000_INT_RXO		(1 << 6)
#define E1000_INT_RXT0		(1 << 7)
#define E1000_INT_MDAC		(1 << 9)
#define E1000_INT_RXCFG		(1 << 10)
#define E1000_INT_PHYINT	(1 << 12)
#define E1000_INT_TXD_LOW	(1 << 15)
#define E1000_INT_SRPD		(1 << 16)


#define E1000_TCTL_EN			(1 << 1)
#define E1000_TCTL_PSP			(1 << 3)
#define E1000_TCTL_CT_SHIFT		4
#define E1000_TCTL_COLD_SHIFT	12
#define E1000_TCTL_SWXOFF		(1 << 22)
#define E1000_TCTL_RTLC			(1 << 24)

#define E1000_RCTL_EN				(1 << 1)
#define E1000_RCTL_SBP				(1 << 2)
#define E1000_RCTL_UPE				(1 << 3)
#define E1000_RCTL_MPE				(1 << 4)
#define E1000_RCTL_LPE				(1 << 5)
#define E1000_RCTL_LBM_NONE			(0 << 6)
#define E1000_RCTL_LBM_PHY			(3 << 6)
#define E1000_RTCL_RDMTS_HALF		(0 << 8)
#define E1000_RTCL_RDMTS_QUARTER	(1 << 8)
#define E1000_RTCL_RDMTS_EIGHTH		(2 << 8)
#define E1000_RCTL_MO_36			(0 << 12)
#define E1000_RCTL_MO_35			(1 << 12)
#define E1000_RCTL_MO_34			(2 << 12)
#define E1000_RCTL_MO_32			(3 << 12)
#define E1000_RCTL_BAM				(1 << 15)
#define E1000_RCTL_VFE				(1 << 18)
#define E1000_RCTL_CFIEN			(1 << 19)
#define E1000_RCTL_CFI				(1 << 20)
#define E1000_RCTL_DPF				(1 << 22)
#define E1000_RCTL_PMCF				(1 << 23)
#define E1000_RCTL_SECRC			(1 << 26)

#define E1000_RCTL_BSIZE_256		(3 << 16)
#define E1000_RCTL_BSIZE_512		(2 << 16)
#define E1000_RCTL_BSIZE_1024		(1 << 16)
#define E1000_RCTL_BSIZE_2048		(0 << 16)
#define E1000_RCTL_BSIZE_4096		((3 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_8192		((2 << 16) | (1 << 25))
#define E1000_RCTL_BSIZE_16384		((1 << 16) | (1 << 25))

namespace Kernel
{

	struct e1000_rx_desc
	{
		volatile uint64_t addr;
		volatile uint16_t length;
		volatile uint16_t checksum;
		volatile uint8_t status;
		volatile uint8_t errors;
		volatile uint16_t special;
	} __attribute__((packed));

	struct e1000_tx_desc
	{
		volatile uint64_t addr;
		volatile uint16_t length;
		volatile uint8_t cso;
		volatile uint8_t cmd;
		volatile uint8_t status;
		volatile uint8_t css;
		volatile uint16_t special;
	} __attribute__((packed));

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

	BAN::ErrorOr<BAN::UniqPtr<E1000>> E1000::create(PCI::Device& pci_device)
	{
		E1000* e1000 = new E1000();
		ASSERT(e1000);
		if (auto ret = e1000->initialize(pci_device); ret.is_error())
		{
			delete e1000;
			return ret.release_error();
		}
		return BAN::UniqPtr<E1000>::adopt(e1000);
	}

	E1000::~E1000()
	{
	}

	BAN::ErrorOr<void> E1000::initialize(PCI::Device& pci_device)
	{
		m_bar_region = TRY(pci_device.allocate_bar_region(0));
		pci_device.enable_bus_mastering();

		detect_eeprom();

		TRY(read_mac_address());
		

		initialize_rx();
		initialize_tx();

		enable_link();
		enable_interrupts();
		
#if DEBUG_E1000
		dprintln("E1000 at PCI {}:{}.{}", pci_device.bus(), pci_device.dev(), pci_device.func());
		dprintln("  MAC: {2H}:{2H}:{2H}:{2H}:{2H}:{2H}",
			m_mac_address[0],
			m_mac_address[1],
			m_mac_address[2],
			m_mac_address[3],
			m_mac_address[4],
			m_mac_address[5]
		);
		dprintln("  link up: {}", link_up());
		if (link_up())
			dprintln("  link speed: {} Mbps", link_speed());
#endif

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
		write32(E1000_REG_EEPROM, 0x01);
		for (int i = 0; i < 1000 && !m_has_eerprom; i++)
			if (read32(E1000_REG_EEPROM) & 0x10)
				m_has_eerprom = true;
	}

	uint32_t E1000::eeprom_read(uint8_t address)
	{
		uint32_t tmp = 0;
		if (m_has_eerprom)
		{
			write32(E1000_REG_EEPROM, ((uint32_t)address << 8) | 1);
			while (!((tmp = read32(E1000_REG_EEPROM)) & (1 << 4)))
				continue;
		}
		else
		{
			write32(E1000_REG_EEPROM, ((uint32_t)address << 2) | 1);
			while (!((tmp = read32(E1000_REG_EEPROM)) & (1 << 1)))
				continue;
		}
		return (tmp >> 16) & 0xFFFF;
	}

	BAN::ErrorOr<void> E1000::read_mac_address()
	{
		if (m_has_eerprom)
		{
			uint32_t temp = eeprom_read(0);
			m_mac_address[0] = temp;
			m_mac_address[1] = temp >> 8;
			
			temp = eeprom_read(1);
			m_mac_address[2] = temp;
			m_mac_address[3] = temp >> 8;
			
			temp = eeprom_read(2);
			m_mac_address[4] = temp;
			m_mac_address[5] = temp >> 8;

			return {};
		}

		if (read32(0x5400) == 0)
		{
			dwarnln("no mac address");
			return BAN::Error::from_errno(EINVAL);
		}

		for (int i = 0; i < 6; i++)
			m_mac_address[i] = (uint8_t)read32(0x5400 + i * 8);

		return {};
	}

	void E1000::initialize_rx()
	{
		uint8_t* ptr = (uint8_t*)kmalloc(sizeof(e1000_rx_desc) * E1000_NUM_RX_DESC + 16, 16, true);
		ASSERT(ptr);

		e1000_rx_desc* descs = (e1000_rx_desc*)ptr;
		for (int i = 0; i < E1000_NUM_RX_DESC; i++)
		{
			// FIXME
			m_rx_descs[i] = &descs[i];
			m_rx_descs[i]->addr = 0;
			m_rx_descs[i]->status = 0;
		}

		write32(E1000_REG_RXDESCLO, (uintptr_t)ptr >> 32);
		write32(E1000_REG_RXDESCHI, (uintptr_t)ptr & 0xFFFFFFFF);
		write32(E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc));
		write32(E1000_REG_RXDESCHEAD, 0);
		write32(E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

		m_rx_current = 0;
		
		uint32_t rctrl = 0;
		rctrl |= E1000_RCTL_EN;
		rctrl |= E1000_RCTL_SBP;
		rctrl |= E1000_RCTL_UPE;
		rctrl |= E1000_RCTL_MPE;
		rctrl |= E1000_RCTL_LBM_NONE;
		rctrl |= E1000_RTCL_RDMTS_HALF;
		rctrl |= E1000_RCTL_BAM;
		rctrl |= E1000_RCTL_SECRC;
		rctrl |= E1000_RCTL_BSIZE_8192;

   		write32(E1000_REG_RCTRL, rctrl);
	}

	void E1000::initialize_tx()
	{
		auto* ptr = (uint8_t*)kmalloc(sizeof(e1000_tx_desc) * E1000_NUM_TX_DESC + 16, 16, true);
		ASSERT(ptr);

		auto* descs = (e1000_tx_desc*)ptr;
		for(int i = 0; i < E1000_NUM_TX_DESC; i++)
		{
			// FIXME
			m_tx_descs[i] = &descs[i];
			m_tx_descs[i]->addr = 0;
			m_tx_descs[i]->cmd = 0;
		}

		write32(E1000_REG_TXDESCHI, (uintptr_t)ptr >> 32);
		write32(E1000_REG_TXDESCLO, (uintptr_t)ptr & 0xFFFFFFFF);
		write32(E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc));
		write32(E1000_REG_TXDESCHEAD, 0);
		write32(E1000_REG_TXDESCTAIL, 0);

		m_tx_current = 0;

		write32(E1000_REG_TCTRL, read32(E1000_REG_TCTRL) | E1000_TCTL_EN | E1000_TCTL_PSP);
		write32(E1000_REG_TIPG, 0x0060200A);
	}

	void E1000::enable_link()
	{
		write32(E1000_REG_CTRL, read32(E1000_REG_CTRL) | E1000_CTRL_SET_LINK_UP);
		m_link_up = !!(read32(E1000_REG_STATUS) & E1000_STATUS_LINK_UP);
	}

	int E1000::link_speed()
	{
		if (!link_up())
			return 0;
		uint32_t speed = read32(E1000_REG_STATUS) & E1000_STATUS_SPEED_MASK;
		if (speed == E1000_STATUS_SPEED_10MB)
			return 10;
		if (speed == E1000_STATUS_SPEED_100MB)
			return 100;
		if (speed == E1000_STATUS_SPEED_1000MB1)
			return 1000;
		if (speed == E1000_STATUS_SPEED_1000MB2)
			return 1000;
		return 0;
	}

	void E1000::enable_interrupts()
	{
		write32(E1000_REG_INT_RATE, 6000);
		write32(E1000_REG_INT_MASK_SET, E1000_INT_LSC | E1000_INT_RXT0 | E1000_INT_RXO);
		read32(E1000_REG_INT_CAUSE_READ);

		// FIXME: implement PCI interrupt allocation
		//IDT::register_irq_handler(irq, E1000::interrupt_handler);
		//InterruptController::enable_irq(irq);
	}

	BAN::ErrorOr<void> E1000::send_packet(const void* data, uint16_t len)
	{
		(void)data;
		(void)len;
		return BAN::Error::from_errno(ENOTSUP);
	}

}
