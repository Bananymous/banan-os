#pragma once

#include <kernel/PCI.h>
#include <kernel/USB/Controller.h>
#include <kernel/USB/Definitions.h>

namespace Kernel
{

	class USBManager
	{
		BAN_NON_COPYABLE(USBManager);
		BAN_NON_MOVABLE(USBManager);

	public:
		static BAN::ErrorOr<void> initialize();
		static USBManager& get();

		BAN::ErrorOr<void> add_controller(PCI::Device& pci_device);

	private:
		USBManager() = default;

	private:
		BAN::Vector<BAN::UniqPtr<USBController>> m_controllers;

		friend class BAN::UniqPtr<USBManager>;
	};

}
