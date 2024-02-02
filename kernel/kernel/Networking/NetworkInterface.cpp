#include <BAN/Endianness.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>

#include <sys/sysmacros.h>
#include <string.h>

namespace Kernel
{

	struct EthernetHeader
	{
		uint8_t dst_mac[6];
		uint8_t src_mac[6];
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

	BAN::ErrorOr<void> NetworkInterface::add_interface_header(BAN::Vector<uint8_t>& packet, uint8_t destination_mac[6])
	{
		ASSERT(m_type == Type::Ethernet);

		TRY(packet.resize(packet.size() + sizeof(EthernetHeader)));
		memmove(packet.data() + sizeof(EthernetHeader), packet.data(), packet.size() - sizeof(EthernetHeader));

		auto* header = reinterpret_cast<EthernetHeader*>(packet.data());
		memcpy(header->dst_mac, destination_mac, 6);
		memcpy(header->src_mac, get_mac_address(), 6);
		header->ether_type = 0x0800; // ipv4

		return {};
	}

}
