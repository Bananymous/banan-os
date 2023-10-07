#pragma once

#include <BAN/UniqPtr.h>
#include <kernel/PCI.h>
#include <kernel/Storage/StorageController.h>
#include <kernel/Storage/ATA/ATABus.h>
#include <kernel/Storage/ATA/ATADevice.h>

namespace Kernel
{

	class ATAController : public StorageController
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<StorageController>> create(PCI::Device&);
		virtual BAN::ErrorOr<void> initialize() override;

	private:
		ATAController(PCI::Device& pci_device)
			: m_pci_device(pci_device)
		{ }

	private:
		PCI::Device& m_pci_device;
	};

}