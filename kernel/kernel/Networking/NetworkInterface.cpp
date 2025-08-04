#include <BAN/Endianness.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>

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
		: CharacterDevice(0400, 0, 0)
		, m_type(type)
		, m_rdev(get_rdev(type))
	{
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

}
