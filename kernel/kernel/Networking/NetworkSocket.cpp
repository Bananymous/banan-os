#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/NetworkSocket.h>

#include <net/if.h>

namespace Kernel
{

	NetworkSocket::NetworkSocket(NetworkLayer& network_layer, const Socket::Info& info)
		: Socket(info)
		, m_network_layer(network_layer)
	{ }

	NetworkSocket::~NetworkSocket()
	{
	}

	void NetworkSocket::bind_interface_and_port(NetworkInterface* interface, uint16_t port)
	{
		ASSERT(!m_interface);
		ASSERT(interface);
		m_interface = interface;
		m_port = port;
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
			case SIOCGIFNAME:
			{
				auto& ifrn_name = ifreq->ifr_ifrn.ifrn_name;
				ASSERT(m_interface->name().size() < sizeof(ifrn_name));
				memcpy(ifrn_name, m_interface->name().data(), m_interface->name().size());
				ifrn_name[m_interface->name().size()] = '\0';
				return 0;
			}
			default:
				return BAN::Error::from_errno(EINVAL);
		}
	}

	BAN::ErrorOr<void> NetworkSocket::getsockname_impl(sockaddr* address, socklen_t* address_len)
	{
		TRY(m_network_layer.get_socket_address(this, address, address_len));
		return {};
	}

}
