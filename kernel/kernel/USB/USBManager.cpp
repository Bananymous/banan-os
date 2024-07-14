#include <BAN/UniqPtr.h>

#include <kernel/USB/USBManager.h>
#include <kernel/USB/XHCI/Controller.h>

namespace Kernel
{

	static BAN::UniqPtr<USBManager> s_instance;

	BAN::ErrorOr<void> USBManager::initialize()
	{
		ASSERT(!s_instance);
		auto manager = TRY(BAN::UniqPtr<USBManager>::create());
		s_instance = BAN::move(manager);

		PCI::PCIManager::get().for_each_device(
			[](PCI::Device& pci_device)
			{
				if (pci_device.class_code() != 0x0C || pci_device.subclass() != 0x03)
					return;
				switch (pci_device.prog_if())
				{
					case 0x00:
						dprintln("Unsupported UHCI controller");
						break;
					case 0x10:
						dprintln("Unsupported OHCI controller");
						break;
					case 0x20:
						dprintln("Unsupported EHCI controller");
						break;
					case 0x30:
						if (auto ret = XHCIController::take_ownership(pci_device); ret.is_error())
							dprintln("Could not take ownership of xHCI controller: {}", ret.error());
						else
							dprintln("Took ownership of xHCI controller");
						break;
					default:
						dprintln("Unsupported USB controller, prog if {2H}", pci_device.prog_if());
						break;
				}
			}
		);

		return {};
	}

	USBManager& USBManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<void> USBManager::add_controller(PCI::Device& pci_device)
	{
		ASSERT(pci_device.class_code() == 0x0C);
		ASSERT(pci_device.subclass() == 0x03);

		BAN::UniqPtr<USBController> controller;
		switch (pci_device.prog_if())
		{
			case 0x00:
				dprintln("Unsupported UHCI controller");
				return BAN::Error::from_errno(ENOTSUP);
			case 0x10:
				dprintln("Unsupported OHCI controller");
				return BAN::Error::from_errno(ENOTSUP);
			case 0x20:
				dprintln("Unsupported EHCI controller");
				return BAN::Error::from_errno(ENOTSUP);
			case 0x30:
				if (auto ret = XHCIController::create(pci_device); ret.is_error())
					dprintln("Could not initialize XHCI controller: {}", ret.error());
				else
					controller = ret.release_value();
				break;
			default:
				dprintln("Unsupported USB controller, prog if {2H}", pci_device.prog_if());
				return BAN::Error::from_errno(EINVAL);
		}

		TRY(m_controllers.push_back(BAN::move(controller)));

		return {};
	}

}
