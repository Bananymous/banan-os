#include <BAN/Endianness.h>
#include <BAN/UniqPtr.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/E1000/E1000.h>
#include <kernel/Networking/E1000/E1000E.h>
#include <kernel/Networking/IPv4.h>
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
		manager->m_arp_table = TRY(ARPTable::create());
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
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);

		if (type != SocketType::DGRAM)
			return BAN::Error::from_errno(EPROTOTYPE);

		auto udp_socket = TRY(UDPSocket::create(mode | Inode::Mode::IFSOCK, uid, gid));
		return BAN::RefPtr<NetworkSocket>(udp_socket);
	}

	void NetworkManager::unbind_socket(uint16_t port, BAN::RefPtr<NetworkSocket> socket)
	{
		if (m_bound_sockets.contains(port))
		{
			ASSERT(m_bound_sockets[port].valid());
			ASSERT(m_bound_sockets[port].lock() == socket);
			m_bound_sockets.remove(port);
		}
		NetworkManager::get().remove_from_cache(socket);
	}

	BAN::ErrorOr<void> NetworkManager::bind_socket(uint16_t port, BAN::RefPtr<NetworkSocket> socket)
	{
		if (m_interfaces.empty())
			return BAN::Error::from_errno(EADDRNOTAVAIL);

		if (port != NetworkSocket::PORT_NONE)
		{
			if (m_bound_sockets.contains(port))
				return BAN::Error::from_errno(EADDRINUSE);
			TRY(m_bound_sockets.insert(port, socket));
		}

		// FIXME: actually determine proper interface
		auto interface = m_interfaces.front();
		socket->bind_interface_and_port(interface.ptr(), port);

		return {};
	}

	void NetworkManager::on_receive(BAN::ConstByteSpan packet)
	{
		// FIXME: properly handle packet parsing

		auto ipv4 = packet.slice(14);
		auto& ipv4_header = ipv4.as<const IPv4Header>();
		auto src_ipv4 = ipv4_header.src_address;

		auto udp = ipv4.slice(20);
		auto& udp_header = udp.as<const UDPHeader>();
		uint16_t src_port = udp_header.src_port;
		uint16_t dst_port = udp_header.dst_port;

		if (!m_bound_sockets.contains(dst_port))
		{
			dprintln("no one is listening on port {}", dst_port);
			return;
		}

		auto raw = udp.slice(8);
		m_bound_sockets[dst_port].lock()->add_packet(raw, src_ipv4, src_port);
	}

}
