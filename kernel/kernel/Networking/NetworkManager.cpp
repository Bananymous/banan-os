#include <BAN/UniqPtr.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/E1000/E1000.h>
#include <kernel/Networking/E1000/E1000E.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/UDPSocket.h>

namespace Kernel
{

	static BAN::UniqPtr<NetworkManager> s_instance;

	BAN::ErrorOr<void> NetworkManager::initialize()
	{
		ASSERT(!s_instance);
		NetworkManager* manager_ptr = new NetworkManager();
		if (manager_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto manager = BAN::UniqPtr<NetworkManager>::adopt(manager_ptr);
		TRY(manager->TmpFileSystem::initialize(0777, 0, 0));
		s_instance = BAN::move(manager);
		return {};
	}

	NetworkManager& NetworkManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	NetworkManager::NetworkManager()
		: TmpFileSystem(128)
	{ }

	BAN::ErrorOr<void> NetworkManager::add_interface(PCI::Device& pci_device)
	{
		BAN::RefPtr<NetworkInterface> interface;

		switch (pci_device.subclass())
		{
			case 0x00:
				if (E1000::probe(pci_device))
				{
					interface = TRY(E1000::create(pci_device));
					break;
				}
				if (E1000E::probe(pci_device))
				{
					interface = TRY(E1000E::create(pci_device));
					break;
				}
				// fall through
			default:
				dprintln("unsupported network controller (pci {2H}.{2H}.{2H})", pci_device.class_code(), pci_device.subclass(), pci_device.prog_if());
				dprintln("  vendor id: {4H}", pci_device.vendor_id());
				dprintln("  device id: {4H}", pci_device.device_id());
				return BAN::Error::from_errno(ENOTSUP);
		}

		ASSERT(interface);

		TRY(m_interfaces.push_back(interface));
		DevFileSystem::get().add_device(interface);

		return {};
	}


	BAN::ErrorOr<BAN::RefPtr<NetworkSocket>> NetworkManager::create_socket(SocketType type, mode_t mode, uid_t uid, gid_t gid)
	{
		if (type != SocketType::DGRAM)
			return BAN::Error::from_errno(EPROTOTYPE);

		auto udp_socket = TRY(UDPSocket::create(mode, uid, gid));
		return BAN::RefPtr<NetworkSocket>(udp_socket);
	}

}
