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

	bool NetworkSocket::can_interface_send_to(const NetworkInterface& interface, const sockaddr* target, socklen_t target_len) const
	{
		ASSERT(target);
		ASSERT(target_len >= static_cast<socklen_t>(sizeof(sockaddr_in)));
		ASSERT(target->sa_family == AF_INET);

		const auto target_ipv4 = BAN::IPv4Address {
			reinterpret_cast<const sockaddr_in*>(target)->sin_addr.s_addr
		};

		switch (interface.type())
		{
			case NetworkInterface::Type::Ethernet:
				// FIXME: this is not really correct :D
				return target_ipv4.octets[0] != IN_LOOPBACKNET;
			case NetworkInterface::Type::Loopback:
				return target_ipv4.octets[0] == IN_LOOPBACKNET;
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<BAN::RefPtr<NetworkInterface>> NetworkSocket::interface(const sockaddr* target, socklen_t target_len)
	{
		ASSERT(m_network_layer.domain() == NetworkSocket::Domain::INET);
		ASSERT(is_bound());

		if (target != nullptr)
		{
			ASSERT(target_len >= static_cast<socklen_t>(sizeof(sockaddr_in)));
			ASSERT(target->sa_family == AF_INET);
		}

		const auto& all_interfaces = NetworkManager::get().interfaces();

		const auto bound_ipv4 = BAN::IPv4Address {
			reinterpret_cast<const sockaddr_in*>(&m_address)->sin_addr.s_addr
		};

		// find the bound interface
		if (bound_ipv4 != 0)
		{
			for (const auto& interface : all_interfaces)
			{
				const auto netmask = interface->get_netmask();
				if (bound_ipv4.mask(netmask) != interface->get_ipv4_address().mask(netmask))
					continue;
				if (target && !can_interface_send_to(*interface, target, target_len))
					continue;
				return interface;
			}

			return BAN::Error::from_errno(EADDRNOTAVAIL);
		}

		// try to find an interface in the same subnet as target
		if (target != nullptr)
		{
			const auto target_ipv4 = BAN::IPv4Address {
				reinterpret_cast<const sockaddr_in*>(target)->sin_addr.s_addr
			};

			for (const auto& interface : all_interfaces)
			{
				const auto netmask = interface->get_netmask();
				if (target_ipv4.mask(netmask) == interface->get_ipv4_address().mask(netmask))
					return interface;
			}
		}

		// return any interface (prefer non-loopback)
		for (const auto& interface : all_interfaces)
			if (interface->type() != NetworkInterface::Type::Loopback)
				if (!target || can_interface_send_to(*interface, target, target_len))
					return interface;
		for (const auto& interface : all_interfaces)
			if (interface->type() == NetworkInterface::Type::Loopback)
				if (!target || can_interface_send_to(*interface, target, target_len))
					return interface;

		return BAN::Error::from_errno(EHOSTUNREACH);
	}

	void NetworkSocket::bind_address_and_port(const sockaddr* addr, socklen_t addr_len)
	{
		ASSERT(!is_bound());
		ASSERT(addr->sa_family != AF_UNSPEC);
		ASSERT(addr_len <= static_cast<socklen_t>(sizeof(sockaddr_storage)));

		memcpy(&m_address, addr, addr_len);
		m_address_len = addr_len;
	}

	BAN::ErrorOr<long> NetworkSocket::ioctl_impl(int request, void* arg)
	{
		if (!arg)
		{
			dprintln("No argument provided");
			return BAN::Error::from_errno(EINVAL);
		}

		auto interface = TRY(this->interface(nullptr, 0));
		auto* ifreq = reinterpret_cast<struct ifreq*>(arg);

		switch (request)
		{
			case SIOCGIFADDR:
			{
				auto& ifru_addr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				ifru_addr.sin_family = AF_INET;
				ifru_addr.sin_addr.s_addr = interface->get_ipv4_address().raw;
				return 0;
			}
			case SIOCSIFADDR:
			{
				auto& ifru_addr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				if (ifru_addr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				interface->set_ipv4_address(BAN::IPv4Address { ifru_addr.sin_addr.s_addr });
				dprintln("IPv4 address set to {}", interface->get_ipv4_address());
				return 0;
			}
			case SIOCGIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				ifru_netmask.sin_family = AF_INET;
				ifru_netmask.sin_addr.s_addr = interface->get_netmask().raw;
				return 0;
			}
			case SIOCSIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				if (ifru_netmask.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				interface->set_netmask(BAN::IPv4Address { ifru_netmask.sin_addr.s_addr });
				dprintln("Netmask set to {}", interface->get_netmask());
				return 0;
			}
			case SIOCGIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				ifru_gwaddr.sin_family = AF_INET;
				ifru_gwaddr.sin_addr.s_addr = interface->get_gateway().raw;
				return 0;
			}
			case SIOCSIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				if (ifru_gwaddr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				interface->set_gateway(BAN::IPv4Address { ifru_gwaddr.sin_addr.s_addr });
				dprintln("Gateway set to {}", interface->get_gateway());
				return 0;
			}
			case SIOCGIFHWADDR:
			{
				auto mac_address = interface->get_mac_address();
				ifreq->ifr_ifru.ifru_hwaddr.sa_family = AF_INET;
				memcpy(ifreq->ifr_ifru.ifru_hwaddr.sa_data, &mac_address, sizeof(mac_address));
				return 0;
			}
			case SIOCGIFNAME:
			{
				auto& ifrn_name = ifreq->ifr_ifrn.ifrn_name;
				ASSERT(interface->name().size() < sizeof(ifrn_name));
				memcpy(ifrn_name, interface->name().data(), interface->name().size());
				ifrn_name[interface->name().size()] = '\0';
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
