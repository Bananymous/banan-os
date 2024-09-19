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
		auto manager = TRY(BAN::UniqPtr<NetworkManager>::create());
		manager->m_ipv4_layer = TRY(IPv4Layer::create());
		s_instance = BAN::move(manager);
		return {};
	}

	NetworkManager& NetworkManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

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

	BAN::ErrorOr<BAN::RefPtr<Socket>> NetworkManager::create_socket(Socket::Domain domain, Socket::Type type, mode_t mode, uid_t uid, gid_t gid)
	{
		switch (domain)
		{
			case Socket::Domain::INET:
				switch (type)
				{
					case Socket::Type::DGRAM:
					case Socket::Type::STREAM:
						break;
					default:
						return BAN::Error::from_errno(EPROTOTYPE);
				}
				break;
			case Socket::Domain::UNIX:
				break;
			default:
				return BAN::Error::from_errno(EAFNOSUPPORT);
		}

		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		mode |= Inode::Mode::IFSOCK;

		auto socket_info = Socket::Info { .mode = mode, .uid = uid, .gid = gid };
		BAN::RefPtr<Socket> socket;
		switch (domain)
		{
			case Socket::Domain::INET:
			{
				switch (type)
				{
					case Socket::Type::DGRAM:
						socket = TRY(UDPSocket::create(*m_ipv4_layer, socket_info));
						break;
					case Socket::Type::STREAM:
						socket = TRY(TCPSocket::create(*m_ipv4_layer, socket_info));
						break;
					default:
						ASSERT_NOT_REACHED();
				}
				break;
			}
			case Socket::Domain::UNIX:
			{
				socket = TRY(UnixDomainSocket::create(type, socket_info));
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
		if (packet.size() < sizeof(EthernetHeader))
			return;
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
