#include <BAN/Endianness.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>

#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <sys/sysmacros.h>
#include <string.h>

namespace Kernel
{

	static BAN::Atomic<dev_t> s_ethernet_rdev_minor = 0;
	static BAN::Atomic<dev_t> s_loopback_rdev_minor = 0;

	static dev_t get_rdev(NetworkInterface::Type type)
	{
		switch (type)
		{
			case NetworkInterface::Type::Ethernet:
				return makedev(DeviceNumber::Ethernet, s_ethernet_rdev_minor++);
			case NetworkInterface::Type::Loopback:
				return makedev(DeviceNumber::Ethernet, s_loopback_rdev_minor++);
		}
		ASSERT_NOT_REACHED();
	}

	NetworkInterface::NetworkInterface(Type type)
		: CharacterDevice(0664, 0, 0)
		, m_type(type)
	{
		m_rdev = get_rdev(type);
		switch (type)
		{
			case Type::Ethernet:
				ASSERT(minor(m_rdev) < 10);
				strcpy(m_name, "ethx");
				m_name[3] = minor(m_rdev) + '0';
				break;
			case Type::Loopback:
				ASSERT(minor(m_rdev) == 0);
				strcpy(m_name, "lo");
				break;
		}
	}

	BAN::ErrorOr<long> NetworkInterface::ioctl_impl(unsigned long request, void* arg)
	{
		if (arg == nullptr)
		{
			dprintln("No argument provided");
			return BAN::Error::from_errno(EINVAL);
		}

		auto* ifreq = reinterpret_cast<struct ifreq*>(arg);

		switch (request)
		{
			case SIOCGIFADDR:
			{
				auto& ifru_addr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				ifru_addr.sin_family = AF_INET;
				ifru_addr.sin_addr.s_addr = get_ipv4_address().raw;
				return 0;
			}
			case SIOCSIFADDR:
			{
				auto& ifru_addr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_addr);
				if (ifru_addr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				set_ipv4_address(BAN::IPv4Address { ifru_addr.sin_addr.s_addr });
				dprintln("IPv4 address set to {}", get_ipv4_address());
				return 0;
			}
			case SIOCGIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				ifru_netmask.sin_family = AF_INET;
				ifru_netmask.sin_addr.s_addr = get_netmask().raw;
				return 0;
			}
			case SIOCSIFNETMASK:
			{
				auto& ifru_netmask = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_netmask);
				if (ifru_netmask.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				set_netmask(BAN::IPv4Address { ifru_netmask.sin_addr.s_addr });
				dprintln("Netmask set to {}", get_netmask());
				return 0;
			}
			case SIOCGIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				ifru_gwaddr.sin_family = AF_INET;
				ifru_gwaddr.sin_addr.s_addr = get_gateway().raw;
				return 0;
			}
			case SIOCSIFGWADDR:
			{
				auto& ifru_gwaddr = *reinterpret_cast<const sockaddr_in*>(&ifreq->ifr_ifru.ifru_gwaddr);
				if (ifru_gwaddr.sin_family != AF_INET)
					return BAN::Error::from_errno(EADDRNOTAVAIL);
				set_gateway(BAN::IPv4Address { ifru_gwaddr.sin_addr.s_addr });
				dprintln("Gateway set to {}", get_gateway());
				return 0;
			}
			case SIOCGIFHWADDR:
			{
				switch (m_type)
				{
					case Type::Loopback:
						ifreq->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_LOOPBACK;
						break;
					case Type::Ethernet:
					{
						const auto mac_address = get_mac_address();
						ifreq->ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
						memcpy(ifreq->ifr_ifru.ifru_hwaddr.sa_data, &mac_address, sizeof(mac_address));
						break;
					}
					default:
						return BAN::Error::from_errno(ENODEV);
				}
				return 0;
			}
			case SIOCGIFNAME:
			{
				auto& ifrn_name = ifreq->ifr_ifrn.ifrn_name;
				ASSERT(name().size() < sizeof(ifrn_name));
				memcpy(ifrn_name, name().data(), name().size());
				ifrn_name[name().size()] = '\0';
				return 0;
			}
			case SIOCGIFFLAGS:
			{
				int flags = 0;
				if (link_up())
					flags |= IFF_UP | IFF_RUNNING;
				if (m_type == Type::Loopback)
					flags |= IFF_LOOPBACK;
				ifreq->ifr_ifru.ifru_flags = flags;
				return 0;
			}
		}

		return CharacterDevice::ioctl_impl(request, arg);
	}


}
