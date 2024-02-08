#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/NetworkSocket.h>

#include <net/if.h>

namespace Kernel
{

	NetworkSocket::NetworkSocket(NetworkLayer& network_layer, ino_t ino, const TmpInodeInfo& inode_info)
		: TmpInode(NetworkManager::get(), ino, inode_info)
		, m_network_layer(network_layer)
	{ }

	NetworkSocket::~NetworkSocket()
	{
	}

	void NetworkSocket::on_close_impl()
	{
		if (m_interface)
			m_network_layer.unbind_socket(m_port, this);
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
		if (m_interface || address_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		auto* addr_in = reinterpret_cast<const sockaddr_in*>(address);
		uint16_t dst_port = BAN::host_to_network_endian(addr_in->sin_port);
		return m_network_layer.bind_socket(dst_port, this);
	}

	BAN::ErrorOr<size_t> NetworkSocket::sendto_impl(const sys_sendto_t* arguments)
	{
		if (arguments->flags)
		{
			dprintln("flags not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (!m_interface)
			TRY(m_network_layer.bind_socket(PORT_NONE, this));

		auto buffer = BAN::ConstByteSpan { reinterpret_cast<const uint8_t*>(arguments->message), arguments->length };
		return TRY(m_network_layer.sendto(*this, buffer, arguments->dest_addr, arguments->dest_len));
	}

	BAN::ErrorOr<size_t> NetworkSocket::recvfrom_impl(sys_recvfrom_t* arguments)
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
				auto& ifru_addr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				ifru_addr.sin_family = AF_INET;
				ifru_addr.sin_addr.s_addr = m_interface->get_ipv4_address().raw;
				return 0;
			}
			case SIOCSIFADDR:
			{
				auto& ifru_addr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				if (ifru_addr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				m_interface->set_ipv4_address(BAN::IPv4Address { ifru_addr.sin_addr.s_addr });
				dprintln("IPv4 address set to {}", m_interface->get_ipv4_address());
				return 0;
			}
			case SIOCGIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				ifru_netmask.sin_family = AF_INET;
				ifru_netmask.sin_addr.s_addr = m_interface->get_netmask().raw;
				return 0;
			}
			case SIOCSIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				if (ifru_netmask.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				m_interface->set_netmask(BAN::IPv4Address { ifru_netmask.sin_addr.s_addr });
				dprintln("Netmask set to {}", m_interface->get_netmask());
				return 0;
			}
			case SIOCGIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				ifru_gwaddr.sin_family = AF_INET;
				ifru_gwaddr.sin_addr.s_addr = m_interface->get_gateway().raw;
				return 0;
			}
			case SIOCSIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				if (ifru_gwaddr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				m_interface->set_gateway(BAN::IPv4Address { ifru_gwaddr.sin_addr.s_addr });
				dprintln("Gateway set to {}", m_interface->get_gateway());
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
