#include <BAN/Endianness.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>

#include <sys/sysmacros.h>
#include <string.h>

namespace Kernel
{

	struct EthernetHeader
	{
		BAN::MACAddress dst_mac;
		BAN::MACAddress src_mac;
		BAN::NetworkEndian<uint16_t> ether_type;
	};
	static_assert(sizeof(EthernetHeader) == 14);

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

	size_t NetworkInterface::interface_header_size() const
	{
		ASSERT(m_type == Type::Ethernet);
		return sizeof(EthernetHeader);
	}

	void NetworkInterface::add_interface_header(BAN::ByteSpan packet, BAN::MACAddress destination)
	{
		ASSERT(m_type == Type::Ethernet);
		auto& header = packet.as<EthernetHeader>();
		header.dst_mac = destination;
		header.src_mac = get_mac_address();
		header.ether_type = 0x0800;
	}

}
