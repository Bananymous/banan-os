#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Storage/ATA/AHCI/Controller.h>
#include <kernel/Storage/ATA/AHCI/Definitions.h>
#include <kernel/Storage/ATA/AHCI/Device.h>

namespace Kernel
{

	BAN::ErrorOr<void> AHCIController::initialize()
	{
		m_abar = TRY(m_pci_device.allocate_bar_region(5));
		if (m_abar->type() != PCI::BarType::MEM)
		{
			dprintln("ABAR not MMIO");
			return BAN::Error::from_errno(EINVAL);
		}

		auto& abar_mem = *(volatile HBAGeneralMemorySpace*)m_abar->vaddr();
		if (!(abar_mem.ghc & SATA_GHC_AHCI_ENABLE))
		{
			dprintln("Controller not in AHCI mode");
			return BAN::Error::from_errno(EINVAL);
		}

		// Enable interrupts and bus mastering
		m_pci_device.enable_bus_mastering();
		TRY(m_pci_device.reserve_irqs(1));
		set_irq(m_pci_device.get_irq(0));
		enable_interrupt();
		abar_mem.ghc = abar_mem.ghc | SATA_GHC_INTERRUPT_ENABLE;

		m_command_slot_count = ((abar_mem.cap >> 8) & 0x1F) + 1;

		uint32_t pi = abar_mem.pi;
		for (uint32_t i = 0; i < 32 && pi; i++, pi >>= 1)
		{
			// Verify we don't access abar outside of its bounds
			if (sizeof(HBAGeneralMemorySpace) + i * sizeof(HBAPortMemorySpace) > m_abar->size())
				break;

			if (!(pi & 1))
				continue;

			auto type = check_port_type(abar_mem.ports[i]);
			if (!type.has_value())
				continue;

			if (type.value() != AHCIPortType::SATA)
			{
				dprintln("Non-SATA devices not supported");
				continue;
			}

			auto device = AHCIDevice::create(this, &abar_mem.ports[i]);
			if (device.is_error())
			{
				dprintln("{}", device.error());
				continue;
			}

			m_devices[i] = device.value().ptr();
			if (auto ret = m_devices[i]->initialize(); ret.is_error())
			{
				dprintln("{}", ret.error());
				m_devices[i] = nullptr;
			}
		}

		return {};
	}

	AHCIController::~AHCIController()
	{
	}

	void AHCIController::handle_irq()
	{
		auto& abar_mem = *(volatile HBAGeneralMemorySpace*)m_abar->vaddr();

		uint32_t is = abar_mem.is;
		for (uint8_t i = 0; i < 32; i++)
		{
			if (is & (1 << i))
			{
				ASSERT(m_devices[i]);
				m_devices[i]->handle_irq();
			}
		}

		abar_mem.is = is;
	}

	BAN::Optional<AHCIPortType> AHCIController::check_port_type(volatile HBAPortMemorySpace& port)
	{
		uint32_t ssts = port.ssts;
		uint8_t ipm = (ssts >> 8) & 0x0F;
		uint8_t det = (ssts >> 0) & 0x0F;

		if (det != HBA_PORT_DET_PRESENT)
			return {};
		if (ipm != HBA_PORT_IPM_ACTIVE)
			return {};

		switch (port.sig)
		{
			case SATA_SIG_ATA:
				return AHCIPortType::SATA;
			case SATA_SIG_ATAPI:
				return AHCIPortType::SATAPI;
			case SATA_SIG_PM:
				return AHCIPortType::PM;
			case SATA_SIG_SEMB:
				return AHCIPortType::SEMB;
		}

		return {};
	}

}
