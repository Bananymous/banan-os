#include <BAN/Endianness.h>
#include <BAN/UniqPtr.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/E1000/E1000.h>
#include <kernel/Networking/E1000/E1000E.h>
#include <kernel/Networking/ICMP.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/TCPSocket.h>
#include <kernel/Networking/UDPSocket.h>
#include <kernel/Networking/UNIX/Socket.h>

#define DEBUG_ETHERTYPE 0

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
		manager->m_ipv4_layer = TRY(IPv4Layer::create());
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

	BAN::ErrorOr<BAN::RefPtr<TmpInode>> NetworkManager::create_socket(SocketDomain domain, SocketType type, mode_t mode, uid_t uid, gid_t gid)
	{
		switch (domain)
		{
			case SocketDomain::INET:
				switch (type)
				{
					case SocketType::DGRAM:
					case SocketType::STREAM:
						break;
					default:
						return BAN::Error::from_errno(EPROTOTYPE);
				}
				break;
			case SocketDomain::UNIX:
				break;
			default:
				return BAN::Error::from_errno(EAFNOSUPPORT);
		}

		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		mode |= Inode::Mode::IFSOCK;

		auto inode_info = create_inode_info(mode, uid, gid);
		ino_t ino = TRY(allocate_inode(inode_info));

		BAN::RefPtr<TmpInode> socket;
		switch (domain)
		{
			case SocketDomain::INET:
			{
				switch (type)
				{
					case SocketType::DGRAM:
						socket = TRY(UDPSocket::create(*m_ipv4_layer, ino, inode_info));
						break;
					case SocketType::STREAM:
						socket = TRY(TCPSocket::create(*m_ipv4_layer, ino, inode_info));
						break;
					default:
						ASSERT_NOT_REACHED();
				}
				break;
			}
			case SocketDomain::UNIX:
			{
				socket = TRY(UnixDomainSocket::create(type, ino, inode_info));
				break;
			}
			default:
				ASSERT_NOT_REACHED();
		}

		ASSERT(socket);
		return socket;
	}

	void NetworkManager::on_receive(NetworkInterface& interface, BAN::ConstByteSpan packet)
	{
		auto ethernet_header = packet.as<const EthernetHeader>();

		switch (ethernet_header.ether_type)
		{
			case EtherType::ARP:
			{
				m_ipv4_layer->arp_table().add_arp_packet(interface, packet.slice(sizeof(EthernetHeader)));
				break;
			}
			case EtherType::IPv4:
			{
				m_ipv4_layer->add_ipv4_packet(interface, packet.slice(sizeof(EthernetHeader)));
				break;
			}
			default:
				dprintln_if(DEBUG_ETHERTYPE, "Unknown EtherType 0x{4H}", (uint16_t)ethernet_header.ether_type);
				break;
		}
	}

}
