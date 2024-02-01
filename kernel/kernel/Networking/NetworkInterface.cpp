#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>

#include <sys/sysmacros.h>
#include <string.h>

namespace Kernel
{

	static dev_t get_network_rdev_major()
	{
		static dev_t major = DevFileSystem::get().get_next_dev();
		return major;
	}

	static dev_t get_network_rdev_minor()
	{
		static dev_t minor = 0;
		return minor++;
	}

	NetworkInterface::NetworkInterface()
		: CharacterDevice(0400, 0, 0)
		, m_type(Type::Ethernet)
		, m_rdev(makedev(get_network_rdev_major(), get_network_rdev_minor()))
	{
		ASSERT(minor(m_rdev) < 10);
		ASSERT(m_type == Type::Ethernet);

		strcpy(m_name, "ethx");
		m_name[3] = minor(m_rdev) + '0';
	}

}
