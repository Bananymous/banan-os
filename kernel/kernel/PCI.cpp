#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Networking/E1000.h>
#include <kernel/PCI.h>
#include <kernel/Storage/ATA/AHCI/Controller.h>
#include <kernel/Storage/ATA/ATAController.h>
#include <kernel/Storage/NVMe/Controller.h>

#define INVALID_VENDOR 0xFFFF
#define MULTI_FUNCTION 0x80

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define PCI_REG_COMMAND 0x04
#define PCI_REG_STATUS 0x06
#define PCI_REG_CAPABILITIES 0x34
#define PCI_REG_IRQ_LINE 0x3C
#define PCI_REG_IRQ_PIN 0x44

#define PCI_CMD_IO_SPACE (1 << 0)
#define PCI_CMD_MEM_SPACE (1 << 1)
#define PCI_CMD_BUS_MASTER (1 << 2)
#define PCI_CMD_INTERRUPT_DISABLE (1 << 10)

#define DEBUG_PCI 0

namespace Kernel::PCI
{

	static PCIManager* s_instance = nullptr;

	struct MSIXEntry
	{
		uint32_t msg_addr_low;
		uint32_t msg_addr_high;
		uint32_t msg_data;
		uint32_t vector_ctrl;
	};
	static_assert(sizeof(MSIXEntry) == 16);

	uint32_t PCIManager::read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		ASSERT(offset % 4 == 0);
		uint32_t config_addr = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | offset;
		IO::outl(CONFIG_ADDRESS, config_addr);
		return IO::inl(CONFIG_DATA);
	}

	uint16_t PCIManager::read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		ASSERT(offset % 2 == 0);
		uint32_t dword = read_config_dword(bus, dev, func, offset & ~3);
		return (dword >> ((offset & 3) * 8)) & 0xFFFF;
	}

	uint8_t PCIManager::read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		uint32_t dword = read_config_dword(bus, dev, func, offset & ~3);
		return (dword >> ((offset & 3) * 8)) & 0xFF;
	}

	void PCIManager::write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value)
	{
		ASSERT(offset % 4 == 0);
		uint32_t config_addr = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | offset;
		IO::outl(CONFIG_ADDRESS, config_addr);
		IO::outl(CONFIG_DATA, value);
	}

	void PCIManager::write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value)
	{
		ASSERT(offset % 2 == 0);
		uint32_t byte = (offset & 3) * 8;
		uint32_t temp = read_config_dword(bus, dev, func, offset & ~3);
		temp &= ~(0xFFFF << byte);
		temp |= (uint32_t)value << byte;
		write_config_dword(bus, dev, func, offset & ~3, temp);
	}

	void PCIManager::write_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value)
	{
		uint32_t byte = (offset & 3) * 8;
		uint32_t temp = read_config_dword(bus, dev, func, offset & ~3);
		temp &= ~(0xFF << byte);
		temp |= (uint32_t)value << byte;
		write_config_dword(bus, dev, func, offset & ~3, temp);
	}

	static uint16_t get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = PCIManager::read_config_dword(bus, dev, func, 0x00);
		return dword & 0xFFFF;
	}

	static uint8_t get_header_type(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = PCIManager::read_config_dword(bus, dev, func, 0x0C);
		return (dword >> 16) & 0xFF;
	}

	void PCIManager::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new PCIManager();
		ASSERT(s_instance);
		s_instance->check_all_buses();
		s_instance->initialize_devices();
	}

	PCIManager& PCIManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void PCIManager::check_function(uint8_t bus, uint8_t dev, uint8_t func)
	{
		MUST(m_devices.emplace_back(bus, dev, func));
		auto& device = m_devices.back();
		if (device.class_code() == 0x06 && device.subclass() == 0x04)
			check_bus(device.read_byte(0x19));
	}

	void PCIManager::check_device(uint8_t bus, uint8_t dev)
	{
		if (get_vendor_id(bus, dev, 0) == INVALID_VENDOR)
			return;

		check_function(bus, dev, 0);
		if (get_header_type(bus, dev, 0) & MULTI_FUNCTION)
			for (uint8_t func = 1; func < 8; func++)
				if (get_vendor_id(bus, dev, func) != INVALID_VENDOR)
					check_function(bus, dev, func);
	}

	void PCIManager::check_bus(uint8_t bus)
	{
		for (uint8_t dev = 0; dev < 32; dev++)
			check_device(bus, dev);
	}

	void PCIManager::check_all_buses()
	{
		if (get_header_type(0, 0, 0) & MULTI_FUNCTION)
		{
			for (int func = 0; func < 8 && get_vendor_id(0, 0, func) != INVALID_VENDOR; func++)
				check_bus(func);
		}
		else
		{
			check_bus(0);
		}
	}

	void PCIManager::initialize_devices()
	{
		for (auto& pci_device : m_devices)
		{
			switch (pci_device.class_code())
			{
				case 0x01:
				{
					switch (pci_device.subclass())
					{
						case 0x01:
						case 0x05:
						case 0x06:
							if (auto res = ATAController::create(pci_device); res.is_error())
								dprintln("ATA: {}", res.error());
							break;
						case 0x08:
							if (auto res = NVMeController::create(pci_device); res.is_error())
								dprintln("NVMe: {}", res.error());
							break;
						default:
							dprintln("unsupported storage device (pci {2H}.{2H}.{2H})", pci_device.class_code(), pci_device.subclass(), pci_device.prog_if());
							break;
					}
					break;
				}
				case 0x02:
				{
					switch (pci_device.subclass())
					{
						case 0x00:
							if (E1000::probe(pci_device))
								if (auto res = E1000::create(pci_device); res.is_error())
									dprintln("E1000: {}", res.error());
							break;
						default:
							dprintln("unsupported ethernet device (pci {2H}.{2H}.{2H})", pci_device.class_code(), pci_device.subclass(), pci_device.prog_if());
							break;
					}
					break;
				}
				default:
					break;
			}
		}
	}

	BAN::ErrorOr<BAN::UniqPtr<BarRegion>> BarRegion::create(PCI::Device& device, uint8_t bar_num)
	{
		if (device.header_type() != 0x00)
		{
			dprintln("BAR regions for non general devices are not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		// disable io/mem space while reading bar
		uint16_t command = device.read_word(PCI_REG_COMMAND);
		device.write_word(PCI_REG_COMMAND, command & ~(PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE));

		uint8_t offset = 0x10 + bar_num * 4;

		uint64_t addr = device.read_dword(offset);

		device.write_dword(offset, 0xFFFFFFFF);
		uint32_t size = device.read_dword(offset);
		size = ~size + 1;
		device.write_dword(offset, addr);

		// determine bar type
		BarType type = BarType::INVALID;
		if (addr & 1)
		{
			type = BarType::IO;
			addr &= 0xFFFFFFFC;
		}
		else if ((addr & 0b110) == 0b000)
		{
			type = BarType::MEM;
			addr &= 0xFFFFFFF0;
		}
		else if ((addr & 0b110) == 0b100)
		{
			type = BarType::MEM;
			addr &= 0xFFFFFFF0;
			addr |= (uint64_t)device.read_dword(offset + 4) << 32;
		}

		if (type == BarType::INVALID)
		{
			dwarnln("invalid pci device bar");
			return BAN::Error::from_errno(EINVAL);
		}

		auto* region_ptr = new BarRegion(type, addr, size);
		ASSERT(region_ptr);

		auto region = BAN::UniqPtr<BarRegion>::adopt(region_ptr);
		TRY(region->initialize());

		// restore old command register and enable correct IO/MEM space
		command |= (type == BarType::IO) ? PCI_CMD_IO_SPACE : PCI_CMD_MEM_SPACE;
		device.write_word(PCI_REG_COMMAND, command);

#if DEBUG_PCI
		dprintln("created BAR region for PCI {}:{}.{}",
			device.bus(),
			device.dev(),
			device.func()
		);
		dprintln("  type: {}", region->type() == BarType::IO ? "IO" : "MEM");
		if (region->type() == BarType::IO)
			dprintln("  iobase {8H}", region->iobase());
		else
		{
			dprintln("  paddr {}", (void*)region->paddr());
			dprintln("  vaddr {}", (void*)region->vaddr());
		}
		dprintln("  size  {}", region->size());
#endif

		return region;
	}

	BarRegion::BarRegion(BarType type, paddr_t paddr, size_t size)
		: m_type(type)
		, m_paddr(paddr)
		, m_size(size)
	{ }

	BarRegion::~BarRegion()
	{
		if (m_type == BarType::MEM && m_vaddr)
			PageTable::kernel().unmap_range(m_vaddr, m_size);
		m_vaddr = 0;
	}

	BAN::ErrorOr<void> BarRegion::initialize()
	{
		if (m_type == BarType::IO)
			return {};

		size_t needed_pages = BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE);
		m_vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
		if (m_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		PageTable::kernel().map_range_at(m_paddr, m_vaddr, m_size, PageTable::Flags::CacheDisable | PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		return {};
	}

	void BarRegion::write8(off_t reg, uint8_t val)
	{
		if (m_type == BarType::IO)
			return IO::outb(m_paddr + reg, val);
		MMIO::write8(m_vaddr + reg, val);
	}

	void BarRegion::write16(off_t reg, uint16_t val)
	{
		if (m_type == BarType::IO)
			return IO::outw(m_paddr + reg, val);
		MMIO::write16(m_vaddr + reg, val);
	}

	void BarRegion::write32(off_t reg, uint32_t val)
	{
		if (m_type == BarType::IO)
			return IO::outl(m_paddr + reg, val);
		MMIO::write32(m_vaddr + reg, val);
	}

	uint8_t BarRegion::read8(off_t reg)
	{
		if (m_type == BarType::IO)
			return IO::inb(m_paddr + reg);
		return MMIO::read8(m_vaddr + reg);
	}

	uint16_t BarRegion::read16(off_t reg)
	{
		if (m_type == BarType::IO)
			return IO::inw(m_paddr + reg);
		return MMIO::read16(m_vaddr + reg);
	}

	uint32_t BarRegion::read32(off_t reg)
	{
		if (m_type == BarType::IO)
			return IO::inl(m_paddr + reg);
		return MMIO::read32(m_vaddr + reg);
	}

	PCI::Device::Device(uint8_t bus, uint8_t dev, uint8_t func)
		: m_bus(bus), m_dev(dev), m_func(func)
	{
		uint32_t type = read_word(0x0A);
		m_class_code  = (uint8_t)(type >> 8);
		m_subclass    = (uint8_t)(type);
		m_prog_if     = read_byte(0x09);
		m_header_type = read_byte(0x0E);

		uint32_t device = read_dword(0x00);
		m_vendor_id = device & 0xFFFF;
		m_device_id = device >> 16;

		dprintln("PCI {2H}:{2H}.{2H} has {2H}.{2H}.{2H}",
			m_bus, m_dev, m_func,
			m_class_code, m_subclass, m_prog_if
		);

		enumerate_capabilites();
	}

	uint32_t PCI::Device::read_dword(uint8_t offset) const
	{
		ASSERT(offset % 4 == 0);
		return PCIManager::read_config_dword(m_bus, m_dev, m_func, offset);
	}

	uint16_t PCI::Device::read_word(uint8_t offset) const
	{
		ASSERT(offset % 2 == 0);
		return PCIManager::read_config_word(m_bus, m_dev, m_func, offset);
	}

	uint8_t PCI::Device::read_byte(uint8_t offset) const
	{
		return PCIManager::read_config_byte(m_bus, m_dev, m_func, offset);
	}

	void PCI::Device::write_dword(uint8_t offset, uint32_t value)
	{
		ASSERT(offset % 4 == 0);
		PCIManager::write_config_dword(m_bus, m_dev, m_func, offset, value);
	}

	void PCI::Device::write_word(uint8_t offset, uint16_t value)
	{
		ASSERT(offset % 2 == 0);
		PCIManager::write_config_word(m_bus, m_dev, m_func, offset, value);
	}

	void PCI::Device::write_byte(uint8_t offset, uint8_t value)
	{
		PCIManager::write_config_byte(m_bus, m_dev, m_func, offset, value);
	}

	BAN::ErrorOr<BAN::UniqPtr<BarRegion>> PCI::Device::allocate_bar_region(uint8_t bar_num)
	{
		return BarRegion::create(*this, bar_num);
	}

	void PCI::Device::enumerate_capabilites()
	{
		uint16_t status = read_word(PCI_REG_STATUS);
		if (!(status & (1 << 4)))
			return;

		uint8_t capability_offset = read_byte(PCI_REG_CAPABILITIES) & 0xFC;
		while (capability_offset)
		{
			uint16_t capability_info = read_word(capability_offset);

			switch (capability_info & 0xFF)
			{
				case 0x05:
					m_offset_msi = capability_offset;
					break;
				case 0x11:
					m_offset_msi_x = capability_offset;
					break;
				default:
					break;
			}

			capability_offset = (capability_info >> 8) & 0xFC;
		}
	}

	BAN::ErrorOr<void> PCI::Device::reserve_irqs(uint8_t count)
	{
		if (m_offset_msi_x.has_value())
		{
			uint16_t msg_ctrl = read_word(*m_offset_msi_x + 0x02);
			if (count > (msg_ctrl & 0x7FF) + 1)
			{
				dwarnln("MSI-X: could not allocate {} interrupts, only {} supported", count, (msg_ctrl & 0x7FF) + 1);
				return BAN::Error::from_errno(EFAULT);
			}
			msg_ctrl |= 1 << 15; // Enable
			write_word(*m_offset_msi_x + 0x02, msg_ctrl);
			disable_pin_interrupts();
		}
		else if (m_offset_msi.has_value())
		{
			if (count > 1)
			{
				dwarnln("MSI: could not allocate {} interrupts, (currently) only {} supported", count, 1);
				return BAN::Error::from_errno(EFAULT);
			}
			uint16_t msg_ctrl = read_word(*m_offset_msi + 0x02);
			msg_ctrl &= ~(0x07 << 4);	// Only one interrupt
			msg_ctrl |= 1u << 0;		// Enable
			write_word(*m_offset_msi + 0x02, msg_ctrl);
			disable_pin_interrupts();
		}
		else if (!InterruptController::get().is_using_apic())
		{
			if (count > 1)
			{
				dwarnln("PIC: could not allocate {} interrupts, (currently) only {} supported", count, 1);
				return BAN::Error::from_errno(EFAULT);
			}
			enable_pin_interrupts();
		}
		else
		{
			dwarnln("Could not reserve interrupt for PCI device. No MSI, MSI-X or interrupt line is used");
			return BAN::Error::from_errno(EFAULT);
		}

		for (; m_reserved_irq_count < count; m_reserved_irq_count++)
		{
			auto irq = InterruptController::get().get_free_irq();
			if (!irq.has_value())
			{
				dwarnln("Could not reserve interrupt for PCI {}:{}.{}", m_bus, m_dev, m_func);
				return BAN::Error::from_errno(EFAULT);
			}

			ASSERT(*irq < 32);
			ASSERT(!(m_reserved_irqs & (1 << *irq)));
			m_reserved_irqs |= 1 << *irq;
		}

		return {};
	}

	static uint64_t msi_message_address()
	{
		return 0xFEE00000;
	}

	static uint32_t msi_message_data(uint8_t irq)
	{
		return (IRQ_VECTOR_BASE + irq) & 0xFF;
	}

	uint8_t PCI::Device::get_irq(uint8_t index)
	{
		ASSERT(m_offset_msi.has_value() || m_offset_msi_x.has_value() || !InterruptController::get().is_using_apic());
		ASSERT(index < m_reserved_irq_count);

		uint8_t count_found = 0;
		uint8_t irq = 0xFF;
		for (uint8_t i = 0; i < 32; i++)
		{
			if (m_reserved_irqs & (1 << i))
				count_found++;
			if (count_found > index)
			{
				irq = i;
				break;
			}
		}
		ASSERT(irq != 0xFF);

		// Legacy PIC just uses the interrupt line field
		if (!InterruptController::get().is_using_apic())
		{
			write_byte(PCI_REG_IRQ_LINE, irq);
			return irq;
		}

		if (m_offset_msi_x.has_value())
		{
			uint32_t dword0 = read_dword(*m_offset_msi_x);
			ASSERT((dword0 & 0xFF) == 0x11);

			uint32_t dword1 = read_dword(*m_offset_msi_x + 0x04);
			uint32_t offset = dword1 & ~3u;
			uint8_t  bir    = dword1 &  3u;

			uint64_t msg_addr = msi_message_address();
			uint32_t msg_data = msi_message_data(irq);

			auto bar = MUST(allocate_bar_region(bir));
			ASSERT(bar->type() == BarType::MEM);
			auto& msi_x_entry = reinterpret_cast<volatile MSIXEntry*>(bar->vaddr() + offset)[index];
			msi_x_entry.msg_addr_low  = msg_addr & 0xFFFFFFFF;
			msi_x_entry.msg_addr_high = msg_addr >> 32;;
			msi_x_entry.msg_data      = msg_data;
			msi_x_entry.vector_ctrl   = msi_x_entry.vector_ctrl & ~1u;

			return irq;
		}

		if (m_offset_msi.has_value())
		{
			uint32_t dword0 = read_dword(*m_offset_msi);
			ASSERT((dword0 & 0xFF) == 0x05);

			uint64_t msg_addr = msi_message_address();
			uint32_t msg_data = msi_message_data(irq);

			if (dword0 & (1 << 23))
			{
				write_dword(*m_offset_msi + 0x04, msg_addr & 0xFFFFFFFF);
				write_dword(*m_offset_msi + 0x08, msg_addr >> 32);
				write_word(*m_offset_msi  + 0x12, msg_data);
			}
			else
			{
				write_dword(*m_offset_msi + 0x04, msg_addr & 0xFFFFFFFF);
				write_word(*m_offset_msi  + 0x08, msg_data);
			}

			return irq;
		}

		ASSERT_NOT_REACHED();
	}

	void PCI::Device::set_command_bits(uint16_t mask)
	{
		write_dword(PCI_REG_COMMAND, read_dword(PCI_REG_COMMAND) | mask);
	}

	void PCI::Device::unset_command_bits(uint16_t mask)
	{
		write_dword(PCI_REG_COMMAND, read_dword(PCI_REG_COMMAND) & ~mask);
	}

	void PCI::Device::enable_bus_mastering()
	{
		set_command_bits(PCI_CMD_BUS_MASTER);
	}

	void PCI::Device::disable_bus_mastering()
	{
		unset_command_bits(PCI_CMD_BUS_MASTER);
	}

	void PCI::Device::enable_memory_space()
	{
		set_command_bits(PCI_CMD_MEM_SPACE);
	}

	void PCI::Device::disable_memory_space()
	{
		unset_command_bits(PCI_CMD_MEM_SPACE);
	}

	void PCI::Device::enable_io_space()
	{
		set_command_bits(PCI_CMD_IO_SPACE);
	}

	void PCI::Device::disable_io_space()
	{
		unset_command_bits(PCI_CMD_IO_SPACE);
	}

	void PCI::Device::enable_pin_interrupts()
	{
		unset_command_bits(PCI_CMD_INTERRUPT_DISABLE);
	}

	void PCI::Device::disable_pin_interrupts()
	{
		set_command_bits(PCI_CMD_INTERRUPT_DISABLE);
	}

}
