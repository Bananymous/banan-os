#pragma once

#include <kernel/Networking/E1000/E1000.h>

namespace Kernel
{

	class E1000E final : public E1000
	{
	public:
		static bool probe(PCI::Device&);
		static BAN::ErrorOr<BAN::RefPtr<E1000E>> create(PCI::Device&);

	protected:
		void detect_eeprom() override;
		uint32_t eeprom_read(uint8_t addr) override;

	private:
		E1000E(PCI::Device& pci_device)
			: E1000(pci_device)
		{ }

	private:
		friend class BAN::RefPtr<E1000E>;
	};

}
