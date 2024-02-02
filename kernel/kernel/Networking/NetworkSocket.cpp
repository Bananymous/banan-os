#include <kernel/Networking/IPv4.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/NetworkSocket.h>

#include <net/if.h>

namespace Kernel
{

	NetworkSocket::NetworkSocket(mode_t mode, uid_t uid, gid_t gid)
		// FIXME: what the fuck is this
		: TmpInode(
			NetworkManager::get(),
			MUST(NetworkManager::get().allocate_inode(create_inode_info(mode, uid, gid))),
			create_inode_info(mode, uid, gid)
		)
	{ }

	NetworkSocket::~NetworkSocket()
	{
	}

	void NetworkSocket::on_close_impl()
	{
		if (m_interface)
			NetworkManager::get().unbind_socket(m_port, this);
	}

	void NetworkSocket::bind_interface_and_port(NetworkInterface* interface, uint16_t port)
	{
		ASSERT(!m_interface);
		ASSERT(interface);
		m_interface = interface;
		m_port = port;
	}

	BAN::ErrorOr<void> NetworkSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		auto* addr_in = reinterpret_cast<const sockaddr_in*>(address);
		return NetworkManager::get().bind_socket(addr_in->sin_port, this);
	}

	BAN::ErrorOr<ssize_t> NetworkSocket::sendto_impl(const sys_sendto_t* arguments)
	{
		if (arguments->dest_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		if (arguments->flags)
		{
			dprintln("flags not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (!m_interface)
			TRY(NetworkManager::get().bind_socket(PORT_NONE, this));

		auto* destination = reinterpret_cast<const sockaddr_in*>(arguments->dest_addr);
		auto  message = BAN::ConstByteSpan((const uint8_t*)arguments->message, arguments->length);

		uint16_t dst_port = destination->sin_port;
		if (dst_port == PORT_NONE)
			return BAN::Error::from_errno(EINVAL);

		auto dst_addr = BAN::IPv4Address(destination->sin_addr.s_addr);
		auto dst_mac = TRY(NetworkManager::get().arp_table().get_mac_from_ipv4(dst_addr));

		const size_t interface_header_offset	= 0;
		const size_t interface_header_size		= m_interface->interface_header_size();

		const size_t ipv4_header_offset	= interface_header_offset + interface_header_size;
		const size_t ipv4_header_size	= sizeof(IPv4Header);

		const size_t protocol_header_offset	= ipv4_header_offset + ipv4_header_size;
		const size_t protocol_header_size	= this->protocol_header_size();

		const size_t payload_offset	= protocol_header_offset + protocol_header_size;
		const size_t payload_size	= message.size();

		BAN::Vector<uint8_t> full_packet;
		TRY(full_packet.resize(payload_offset + payload_size));

		BAN::ByteSpan packet_bytespan { full_packet.span() };

		memcpy(full_packet.data() + payload_offset, message.data(), payload_size);
		add_protocol_header(packet_bytespan.slice(protocol_header_offset), m_port, dst_port);
		add_ipv4_header(packet_bytespan.slice(ipv4_header_offset), m_interface->get_ipv4_address(), dst_addr, protocol());
		m_interface->add_interface_header(packet_bytespan.slice(interface_header_offset), dst_mac);
		TRY(m_interface->send_raw_bytes(packet_bytespan));

		return arguments->length;
	}

	BAN::ErrorOr<ssize_t> NetworkSocket::recvfrom_impl(sys_recvfrom_t* arguments)
	{
		sockaddr_in* sender_addr = nullptr;
		if (arguments->address)
		{
			ASSERT(arguments->address_len);
			if (*arguments->address_len < (socklen_t)sizeof(sockaddr_in))
				*arguments->address_len = 0;
			else
			{
				sender_addr = reinterpret_cast<sockaddr_in*>(arguments->address);
				*arguments->address_len = sizeof(sockaddr_in);
			}
		}

		if (!m_interface)
		{
			dprintln("No interface bound");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_port == PORT_NONE)
		{
			dprintln("No port bound");
			return BAN::Error::from_errno(EINVAL);
		}

		return TRY(read_packet(BAN::ByteSpan { reinterpret_cast<uint8_t*>(arguments->buffer), arguments->length }, sender_addr));
	}

	BAN::ErrorOr<long> NetworkSocket::ioctl_impl(int request, void* arg)
	{
		if (!arg)
		{
			dprintln("No argument provided");
			return BAN::Error::from_errno(EINVAL);
		}
		if (m_interface == nullptr)
		{
			dprintln("No interface bound");
			return BAN::Error::from_errno(EADDRNOTAVAIL);
		}

		auto* ifreq = reinterpret_cast<struct ifreq*>(arg);

		switch (request)
		{
			case SIOCGIFADDR:
			{
				auto ipv4_address = m_interface->get_ipv4_address();
				ifreq->ifr_ifru.ifru_addr.sa_family = AF_INET;
				memcpy(ifreq->ifr_ifru.ifru_addr.sa_data, &ipv4_address, sizeof(ipv4_address));
				return 0;
			}
			case SIOCSIFADDR:
			{
				if (ifreq->ifr_ifru.ifru_addr.sa_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				BAN::IPv4Address ipv4_address { *reinterpret_cast<uint32_t*>(ifreq->ifr_ifru.ifru_addr.sa_data) };
				m_interface->set_ipv4_address(ipv4_address);
				dprintln("IPv4 address set to {}", m_interface->get_ipv4_address());
				return 0;
			}
			case SIOCGIFNETMASK:
			{
				auto netmask_address = m_interface->get_netmask();
				ifreq->ifr_ifru.ifru_netmask.sa_family = AF_INET;
				memcpy(ifreq->ifr_ifru.ifru_netmask.sa_data, &netmask_address, sizeof(netmask_address));
				return 0;
			}
			case SIOCSIFNETMASK:
			{
				if (ifreq->ifr_ifru.ifru_netmask.sa_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				BAN::IPv4Address netmask { *reinterpret_cast<uint32_t*>(ifreq->ifr_ifru.ifru_netmask.sa_data) };
				m_interface->set_netmask(netmask);
				dprintln("Netmask set to {}", m_interface->get_netmask());
				return 0;
			}
			case SIOCGIFHWADDR:
			{
				auto mac_address = m_interface->get_mac_address();
				ifreq->ifr_ifru.ifru_hwaddr.sa_family = AF_INET;
				memcpy(ifreq->ifr_ifru.ifru_hwaddr.sa_data, &mac_address, sizeof(mac_address));
				return 0;
			}
			default:
				return BAN::Error::from_errno(EINVAL);
		}
	}

}
