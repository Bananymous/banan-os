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
				if (auto ret = XHCIController::initialize(pci_device); ret.is_error())
					dprintln("Could not initialize XHCI controller: {}", ret.error());
				else
					controller = ret.release_value();
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		TRY(m_controllers.push_back(BAN::move(controller)));

		return {};
	}

}
