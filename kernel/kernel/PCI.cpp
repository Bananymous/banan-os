#include <BAN/ScopeGuard.h>

#include <kernel/APIC.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/PCI.h>
#include <kernel/Storage/ATA/AHCI/Controller.h>
#include <kernel/Storage/ATA/ATAController.h>
#include <kernel/Storage/NVMe/Controller.h>
#include <kernel/USB/USBManager.h>

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
		return m_buses[bus][dev][func].read_dword(offset);
	}

	uint16_t PCIManager::read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		return m_buses[bus][dev][func].read_word(offset);
	}

	uint8_t PCIManager::read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		return m_buses[bus][dev][func].read_byte(offset);
	}

	void PCIManager::write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value)
	{
		m_buses[bus][dev][func].write_dword(offset, value);
	}

	void PCIManager::write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value)
	{
		m_buses[bus][dev][func].write_word(offset, value);
	}

	void PCIManager::write_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value)
	{
		m_buses[bus][dev][func].write_byte(offset, value);
	}

	static uint16_t get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = PCIManager::get().read_config_dword(bus, dev, func, 0x00);
		return dword & 0xFFFF;
	}

	static uint8_t get_header_type(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = PCIManager::get().read_config_dword(bus, dev, func, 0x0C);
		return (dword >> 16) & 0xFF;
	}

	void PCIManager::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new PCIManager();
		ASSERT(s_instance);
		s_instance->initialize_impl();
	}

	void PCIManager::initialize_impl()
	{
		struct BAAS
		{
			uint64_t addr;
			uint16_t segment;
			uint8_t bus_start;
			uint8_t bus_end;
			uint32_t __reserved;
		};
		static_assert(sizeof(BAAS) == 16);

		if (auto* mcfg = ACPI::ACPI::get().get_header("MCFG", 0))
		{
			const size_t count = (mcfg->length - 44) / 16;
			const BAAS* baas = reinterpret_cast<BAAS*>(reinterpret_cast<vaddr_t>(mcfg) + 44);
			for (size_t i = 0; i < count; i++, baas++)
			{
				// FIXME: support all segments
				if (baas->segment != 0)
					continue;
				for (uint64_t bus = baas->bus_start; bus <= baas->bus_end; bus++)
				{
					ASSERT(m_bus_pcie_paddr[bus] == 0);
					m_bus_pcie_paddr[bus] = baas->addr + (bus << 20);
				}
			}

			m_is_pcie = true;
		}

		for (size_t bus = 0; bus < m_buses.size(); bus++)
			for (size_t dev = 0; dev < m_buses[bus].size(); dev++)
				for (size_t func = 0; func < m_buses[bus][dev].size(); func++)
					m_buses[bus][dev][func].set_location(bus, dev, func);
		s_instance->check_all_buses();
	}

	PCIManager& PCIManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void PCIManager::check_function(uint8_t bus, uint8_t dev, uint8_t func)
	{
		auto& device = m_buses[bus][dev][func];
		const paddr_t pcie_paddr = m_is_pcie ? m_bus_pcie_paddr[bus] + (((paddr_t)dev << 15) | ((paddr_t)func << 12)) : 0;
		device.initialize(pcie_paddr);
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

	BAN::Optional<uint8_t> PCIManager::reserve_msi()
	{
		SpinLockGuard _(m_reserved_msi_lock);

		for (uint8_t i = 0; i < m_msi_count; i++)
		{
			const uint8_t byte = i / 8;
			const uint8_t bit  = i % 8;
			if (m_reserved_msi_bitmap[byte] & (1 << bit))
				continue;
			m_reserved_msi_bitmap[byte] |= 1 << bit;
			return IRQ_MSI_BASE - IRQ_VECTOR_BASE + i;
		}

		return {};
	}

	void PCIManager::initialize_devices(bool disable_usb)
	{
		for_each_device(
			[&](PCI::Device& pci_device)
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
						if (auto res = NetworkManager::get().add_interface(pci_device); res.is_error())
							dprintln("{}", res.error());
						break;
					}
					case 0x0C:
					{
						switch (pci_device.subclass())
						{
							case 0x03:
								if (disable_usb)
									dprintln("USB support disabled, will not initialize {2H}.{2H}.{2H}", pci_device.class_code(), pci_device.subclass(), pci_device.prog_if());
								else if (auto res = USBManager::get().add_controller(pci_device); res.is_error())
									dprintln("{}", res.error());
								break;
							default:
								dprintln("unsupported serial bus controller (pci {2H}.{2H}.{2H})", pci_device.class_code(), pci_device.subclass(), pci_device.prog_if());
								break;
						}
						break;
					}
					default:
						break;
				}
			}
		);
	}

	void PCI::Device::set_location(uint8_t bus, uint8_t dev, uint8_t func)
	{
		m_bus = bus;
		m_dev = dev;
		m_func = func;
	}

	void PCI::Device::initialize(paddr_t pcie_paddr)
	{
		m_is_valid = true;

		if (pcie_paddr)
		{
			vaddr_t vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
			ASSERT(vaddr);
			PageTable::kernel().map_page_at(pcie_paddr, vaddr, PageTable::Flags::ReadWrite | PageTable::Flags::Present, PageTable::MemoryType::Uncached);
			m_mmio_config = vaddr;
		}

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
		if (m_mmio_config)
			return MMIO::read32(m_mmio_config + offset);
		uint32_t config_addr = 0x80000000 | ((uint32_t)m_bus << 16) | ((uint32_t)m_dev << 11) | ((uint32_t)m_func << 8) | offset;
		IO::outl(CONFIG_ADDRESS, config_addr);
		return IO::inl(CONFIG_DATA);
	}

	uint16_t PCI::Device::read_word(uint8_t offset) const
	{
		ASSERT(offset % 2 == 0);
		if (m_mmio_config)
			return MMIO::read16(m_mmio_config + offset);
		uint32_t dword = read_dword(offset & ~3);
		return (dword >> ((offset & 3) * 8)) & 0xFFFF;
	}

	uint8_t PCI::Device::read_byte(uint8_t offset) const
	{
		if (m_mmio_config)
			return MMIO::read8(m_mmio_config + offset);
		uint32_t dword = read_dword(offset & ~3);
		return (dword >> ((offset & 3) * 8)) & 0xFF;
	}

	void PCI::Device::write_dword(uint8_t offset, uint32_t value)
	{
		ASSERT(offset % 4 == 0);
		if (m_mmio_config)
			return MMIO::write32(m_mmio_config + offset, value);
		uint32_t config_addr = 0x80000000 | ((uint32_t)m_bus << 16) | ((uint32_t)m_dev << 11) | ((uint32_t)m_func << 8) | offset;
		IO::outl(CONFIG_ADDRESS, config_addr);
		IO::outl(CONFIG_DATA, value);
	}

	void PCI::Device::write_word(uint8_t offset, uint16_t value)
	{
		ASSERT(offset % 2 == 0);
		if (m_mmio_config)
			return MMIO::write16(m_mmio_config + offset, value);
		uint32_t byte = (offset & 3) * 8;
		uint32_t temp = read_dword(offset & ~3);
		temp &= ~(0xFFFF << byte);
		temp |= (uint32_t)value << byte;
		write_dword(offset & ~3, temp);
	}

	void PCI::Device::write_byte(uint8_t offset, uint8_t value)
	{
		if (m_mmio_config)
			return MMIO::write8(m_mmio_config + offset, value);
		uint32_t byte = (offset & 3) * 8;
		uint32_t temp = read_dword(offset & ~3);
		temp &= ~(0xFF << byte);
		temp |= (uint32_t)value << byte;
		write_dword(offset & ~3, temp);
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

	uint8_t PCI::Device::get_interrupt(uint8_t index) const
	{
		ASSERT(m_interrupt_mechanism != InterruptMechanism::NONE);
		ASSERT(index < m_reserved_interrupt_count);

		uint8_t count_found = 0;
		for (uint32_t irq = 0; irq < 0x100; irq++)
		{
			const uint8_t byte = irq / 8;
			const uint8_t bit  = irq % 8;

			if (m_reserved_interrupts[byte] & (1 << bit))
				count_found++;
			if (index + 1 == count_found)
				return irq;
		}

		ASSERT_NOT_REACHED();
	}

	static constexpr uint64_t msi_message_address()
	{
		return 0xFEE00000;
	}

	static constexpr uint32_t msi_message_data(uint8_t irq)
	{
		return (IRQ_VECTOR_BASE + irq) & 0xFF;
	}

	void PCI::Device::enable_interrupt(uint8_t index, Interruptable& interruptable)
	{
		const uint8_t irq = get_interrupt(index);
		interruptable.set_irq(irq);

		const auto disable_msi =
			[this]()
			{
				if (!m_offset_msi.has_value())
					return;
				uint16_t msg_ctrl = read_word(*m_offset_msi + 0x02);
				msg_ctrl &= ~(1u << 0);
				write_word(*m_offset_msi + 0x02, msg_ctrl);
			};

		const auto disable_msi_x =
			[this]()
			{
				if (!m_offset_msi_x.has_value())
					return;
				uint16_t msg_ctrl = read_word(*m_offset_msi_x + 0x02);
				msg_ctrl &= ~(1u << 15); // Disable
				write_word(*m_offset_msi_x + 0x02, msg_ctrl);
			};

		switch (m_interrupt_mechanism)
		{
			case InterruptMechanism::NONE:
				ASSERT_NOT_REACHED();
			case InterruptMechanism::PIN:
				enable_pin_interrupts();
				disable_msi();
				disable_msi_x();

				if (!InterruptController::get().is_using_apic())
					write_byte(PCI_REG_IRQ_LINE, irq);
				InterruptController::get().enable_irq(irq);
				break;
			case InterruptMechanism::MSI:
			{
				disable_pin_interrupts();
				disable_msi_x();

				uint16_t msg_ctrl = read_word(*m_offset_msi + 0x02);
				msg_ctrl &= ~(0x07 << 4);	// Only one interrupt
				msg_ctrl |= 1u << 0;		// Enable
				write_word(*m_offset_msi + 0x02, msg_ctrl);

				const uint64_t msg_addr = msi_message_address();
				const uint32_t msg_data = msi_message_data(irq);

				if (msg_ctrl & (1 << 7))
				{
					write_dword(*m_offset_msi + 0x04, msg_addr & 0xFFFFFFFF);
					write_dword(*m_offset_msi + 0x08, msg_addr >> 32);
					write_word(*m_offset_msi  + 0x0C, msg_data);
				}
				else
				{
					write_dword(*m_offset_msi + 0x04, msg_addr & 0xFFFFFFFF);
					write_word(*m_offset_msi  + 0x08, msg_data);
				}

				break;
			}
			case InterruptMechanism::MSIX:
			{
				disable_pin_interrupts();
				disable_msi();

				uint16_t msg_ctrl = read_word(*m_offset_msi_x + 0x02);
				msg_ctrl |= 1 << 15; // Enable
				write_word(*m_offset_msi_x + 0x02, msg_ctrl);

				const uint32_t dword1 = read_dword(*m_offset_msi_x + 0x04);
				const uint32_t offset = dword1 & ~7u;
				const uint8_t  bir    = dword1 &  7u;

				const uint64_t msg_addr = msi_message_address();
				const uint32_t msg_data = msi_message_data(irq);

				auto bar = MUST(allocate_bar_region(bir));
				ASSERT(bar->type() == BarType::MEM);

				auto& msi_x_entry = reinterpret_cast<volatile MSIXEntry*>(bar->vaddr() + offset)[index];
				msi_x_entry.msg_addr_low  = msg_addr & 0xFFFFFFFF;
				msi_x_entry.msg_addr_high = msg_addr >> 32;;
				msi_x_entry.msg_data      = msg_data;
				msi_x_entry.vector_ctrl   = msi_x_entry.vector_ctrl & ~1u;

				break;
			}
		}
	}

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstack-usage="
#endif
	BAN::ErrorOr<uint8_t> PCI::Device::route_prt_entry(const ACPI::AML::Node& prt_entry)
	{
		ASSERT(prt_entry.type == ACPI::AML::Node::Type::Package);
		ASSERT(prt_entry.as.package->num_elements == 4);
		for (size_t i = 0; i < 4; i++)
			ASSERT(prt_entry.as.package->elements[i].value.node);

		auto& prt_entry_fields = prt_entry.as.package->elements;

		auto& source_node = *prt_entry_fields[2].value.node;
		if (source_node.type != ACPI::AML::Node::Type::Reference || source_node.as.reference->node.type != ACPI::AML::Node::Type::Device)
		{
			BAN::ScopeGuard debug_guard([] { dwarnln("unknown or invalid _PRT format"); });

			const auto source_value = TRY(ACPI::AML::convert_node(TRY(source_node.copy()), ACPI::AML::ConvInteger, -1)).as.integer.value;
			if (source_value != 0x00)
				return BAN::Error::from_errno(EINVAL);

			auto& gsi_node = *prt_entry_fields[3].value.node;
			const auto gsi_value = TRY(ACPI::AML::convert_node(TRY(gsi_node.copy()), ACPI::AML::ConvInteger, -1)).as.integer.value;

			debug_guard.disable();

			auto& apic = static_cast<APIC&>(InterruptController::get());
			return TRY(apic.reserve_gsi(gsi_value));
		}
		else
		{
			BAN::ScopeGuard debug_guard([] { dwarnln("unknown or invalid _PRT format"); });

			auto& acpi_namespace = *ACPI::ACPI::get().acpi_namespace();

			auto source_scope = TRY(acpi_namespace.find_reference_scope(source_node.as.reference));

			auto crs_node = TRY(ACPI::AML::convert_node(TRY(acpi_namespace.evaluate(source_scope, "_CRS"_sv)), ACPI::AML::ConvBuffer, -1));

			auto crs_buffer = BAN::ConstByteSpan(crs_node.as.str_buf->bytes, crs_node.as.str_buf->size);
			while (!crs_buffer.empty())
			{
				if (!(crs_buffer[0] & 0x80))
				{
					const uint8_t name = ((crs_buffer[0] >> 3) & 0x0F);
					const uint8_t length = (crs_buffer[0] & 0x07);
					if (crs_buffer.size() < static_cast<size_t>(1 + length))
						return BAN::Error::from_errno(EINVAL);

					// IRQ Format Descriptor
					if (name == 0x04)
					{
						if (length < 2)
							return BAN::Error::from_errno(EINVAL);

						const uint16_t irq_mask = crs_buffer[1] | (crs_buffer[2] << 8);
						if (irq_mask == 0)
							return BAN::Error::from_errno(EINVAL);

						debug_guard.disable();

						uint8_t irq;
						for (irq = 0; irq < 16; irq++)
							if (irq_mask & (1 << irq))
								break;

						if (auto ret = InterruptController::get().reserve_irq(irq); ret.is_error())
						{
							dwarnln("FIXME: irq sharing");
							return ret.release_error();
						}

						return irq;
					}

					crs_buffer = crs_buffer.slice(1 + length);
				}
				else
				{
					if (crs_buffer.size() < 3)
						return BAN::Error::from_errno(EINVAL);
					const uint8_t  name = (crs_buffer[0] & 0x7F);
					const uint16_t length = (crs_buffer[2] << 8) | crs_buffer[1];
					if (crs_buffer.size() < static_cast<size_t>(3 + length))
						return BAN::Error::from_errno(EINVAL);

					// Extended Interrupt Descriptor
					if (name == 0x09)
					{
						if (length < 6 || crs_buffer[4] != 1)
							return BAN::Error::from_errno(EINVAL);

						const uint32_t irq  =
							(static_cast<uint32_t>(crs_buffer[5]) <<  0) |
							(static_cast<uint32_t>(crs_buffer[6]) <<  8) |
							(static_cast<uint32_t>(crs_buffer[7]) << 16) |
							(static_cast<uint32_t>(crs_buffer[8]) << 24);

						debug_guard.disable();

						if (auto ret = InterruptController::get().reserve_irq(irq); ret.is_error())
						{
							dwarnln("FIXME: irq sharing");
							return ret.release_error();
						}

						return irq;
					}

					crs_buffer = crs_buffer.slice(3 + length);
				}
			}
		}

		return BAN::Error::from_errno(EFAULT);
	}
#pragma GCC diagnostic pop

	static BAN::ErrorOr<ACPI::AML::Scope> find_pci_bus(uint16_t seg, uint8_t bus)
	{
		constexpr BAN::StringView pci_root_bus_ids[] {
			"PNP0A03"_sv, // PCI
			"PNP0A08"_sv, // PCIe
		};

		ASSERT(ACPI::ACPI::get().acpi_namespace());
		auto& acpi_namespace = *ACPI::ACPI::get().acpi_namespace();

		for (const auto eisa_id : pci_root_bus_ids)
		{
			auto root_buses = TRY(acpi_namespace.find_device_with_eisa_id(eisa_id));

			for (const auto& root_bus : root_buses)
			{
				uint64_t bbn_value = 0;
				if (auto bbn_node_or_error = acpi_namespace.evaluate(root_bus, "_BBN"_sv); !bbn_node_or_error.is_error())
					bbn_value = TRY(ACPI::AML::convert_node(bbn_node_or_error.release_value(), ACPI::AML::ConvInteger, -1)).as.integer.value;

				uint64_t seg_value = 0;
				if (auto seg_node_or_error = acpi_namespace.evaluate(root_bus, "_SEG"_sv); !seg_node_or_error.is_error())
					seg_value = TRY(ACPI::AML::convert_node(seg_node_or_error.release_value(), ACPI::AML::ConvInteger, -1)).as.integer.value;

				if (seg_value == seg && bbn_value == bus)
					return TRY(root_bus.copy());
			}
		}

		return BAN::Error::from_errno(ENOENT);
	}

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstack-usage="
#endif
	// TODO: maybe move this code to ACPI related file?
	BAN::ErrorOr<uint8_t> PCI::Device::find_intx_interrupt()
	{
		ASSERT(InterruptController::get().is_using_apic());

		const uint32_t acpi_device_id = (static_cast<uint32_t>(m_dev) << 16) | 0xFFFF;
		const uint8_t acpi_pin = read_byte(0x3D) - 1;
		if (acpi_pin > 0x03)
		{
			dwarnln("PCI device is not using PIN interrupts");
			return BAN::Error::from_errno(EINVAL);
		}

		if (ACPI::ACPI::get().acpi_namespace() == nullptr)
			return BAN::Error::from_errno(EFAULT);
		auto& acpi_namespace = *ACPI::ACPI::get().acpi_namespace();

		// FIXME: support segments
		auto pci_root_bus = TRY(find_pci_bus(0, m_bus));

		auto prt_node = TRY(acpi_namespace.evaluate(pci_root_bus, "_PRT"));
		if (prt_node.type != ACPI::AML::Node::Type::Package)
		{
			dwarnln("{}\\_PRT did not evaluate to package");
			return BAN::Error::from_errno(EINVAL);
		}

		for (size_t i = 0; i < prt_node.as.package->num_elements; i++)
		{
			if (ACPI::AML::resolve_package_element(prt_node.as.package->elements[i], true).is_error())
				continue;

			auto& prt_entry = *prt_node.as.package->elements[i].value.node;
			if (prt_entry.type != ACPI::AML::Node::Type::Package)
				continue;
			if (prt_entry.as.package->num_elements != 4)
				continue;

			bool resolved = true;
			for (size_t j = 0; j < 4 && resolved; j++)
				if (ACPI::AML::resolve_package_element(prt_entry.as.package->elements[j], true).is_error())
					resolved = false;
			if (!resolved)
				continue;

			auto& prt_entry_fields = prt_entry.as.package->elements;
			if (TRY(ACPI::AML::convert_node(TRY(prt_entry_fields[0].value.node->copy()), ACPI::AML::ConvInteger, -1)).as.integer.value != acpi_device_id)
				continue;
			if (TRY(ACPI::AML::convert_node(TRY(prt_entry_fields[1].value.node->copy()), ACPI::AML::ConvInteger, -1)).as.integer.value != acpi_pin)
				continue;

			auto ret = route_prt_entry(prt_entry);
			if (!ret.is_error())
				return ret;
		}

		dwarnln("No routable PCI interrupt found");
		return BAN::Error::from_errno(EFAULT);
	}
#pragma GCC diagnostic pop

	BAN::ErrorOr<void> PCI::Device::reserve_interrupts(uint8_t count)
	{
		// FIXME: Allow "late" interrupt reserving
		ASSERT(m_reserved_interrupt_count == 0);

		const auto mechanism =
			[this, count]() -> InterruptMechanism
			{
				if (!InterruptController::get().is_using_apic())
				{
					// FIXME: support multiple PIN interrupts
					if (count == 1)
						return InterruptMechanism::PIN;

					// MSI cannot be used without LAPIC
					return InterruptMechanism::NONE;
				}

				if (m_offset_msi_x.has_value())
				{
					const uint16_t msg_ctrl = read_word(*m_offset_msi_x + 0x02);
					if (count <= (msg_ctrl & 0x7FF) + 1)
						return InterruptMechanism::MSIX;
				}

				if (m_offset_msi.has_value())
				{
					if (count == 1)
						return InterruptMechanism::MSI;
					// FIXME: support multiple MSIs
				}

				if (count == 1)
					return InterruptMechanism::PIN;
				return InterruptMechanism::NONE;
			}();

		if (mechanism == InterruptMechanism::NONE)
		{
			dwarnln("No supported interrupt mechanism available");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto get_interrupt_func =
			[this, mechanism]() -> BAN::Optional<uint8_t>
			{
				switch (mechanism)
				{
					case InterruptMechanism::NONE:
						ASSERT_NOT_REACHED();
					case InterruptMechanism::PIN:
						if (!InterruptController::get().is_using_apic())
							return InterruptController::get().get_free_irq();
						if (auto ret = find_intx_interrupt(); !ret.is_error())
							return ret.release_value();
						return {};
					case InterruptMechanism::MSI:
					case InterruptMechanism::MSIX:
						return PCIManager::get().reserve_msi();
				}
				ASSERT_NOT_REACHED();
			};

		for (uint8_t i = 0; i < count; i++)
		{
			const auto irq = get_interrupt_func();
			if (!irq.has_value())
			{
				dwarnln("Could not reserve {} interrupts", count);
				return BAN::Error::from_errno(EFAULT);
			}
			const uint8_t byte = irq.value() / 8;
			const uint8_t bit  = irq.value() % 8;
			m_reserved_interrupts[byte] |= 1 << bit;
		}

		m_interrupt_mechanism = mechanism;
		m_reserved_interrupt_count = count;

		return {};
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
		dprintln("created BAR region for PCI {2H}:{2H}.{2H}",
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

		PageTable::kernel().map_range_at(
			m_paddr, m_vaddr, m_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			PageTable::MemoryType::Uncached
		);

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

}
