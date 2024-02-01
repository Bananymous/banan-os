#include <kernel/Networking/E1000/E1000E.h>
#include <kernel/Timer/Timer.h>

#define E1000E_VENDOR_INTEL 0x8086
#define E1000E_DEVICE_82574 0x10D3

namespace Kernel
{

	bool E1000E::probe(PCI::Device& pci_device)
	{
		if (pci_device.vendor_id() != E1000E_VENDOR_INTEL)
			return false;
		switch (pci_device.device_id())
		{
			case E1000E_DEVICE_82574:
				return true;
		}
		return false;
	}

	BAN::ErrorOr<BAN::RefPtr<E1000E>> E1000E::create(PCI::Device& pci_device)
	{
		auto e1000e = TRY(BAN::RefPtr<E1000E>::create(pci_device));
		TRY(e1000e->initialize());
		return e1000e;
	}

	void E1000E::detect_eeprom()
	{
		m_has_eerprom = true;
	}

	uint32_t E1000E::eeprom_read(uint8_t addr)
	{
		uint32_t tmp;
		write32(REG_EERD, ((uint32_t)addr << 2) | 1);
		while (!((tmp = read32(REG_EERD)) & (1 << 1)))
			continue;
		return tmp >> 16;
	}

}
