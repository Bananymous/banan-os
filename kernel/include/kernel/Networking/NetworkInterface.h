#pragma once

#include <BAN/Errors.h>
#include <BAN/ByteSpan.h>
#include <BAN/MAC.h>
#include <kernel/Device/Device.h>
#include <kernel/Networking/IPv4.h>

namespace Kernel
{

	struct EthernetHeader
	{
		BAN::MACAddress dst_mac;
		BAN::MACAddress src_mac;
		BAN::NetworkEndian<uint16_t> ether_type;
	};
	static_assert(sizeof(EthernetHeader) == 14);

	enum EtherType : uint16_t
	{
		IPv4 = 0x0800,
		ARP = 0x0806,
	};

	class NetworkInterface : public CharacterDevice
	{
		BAN_NON_COPYABLE(NetworkInterface);
		BAN_NON_MOVABLE(NetworkInterface);

	public:
		enum class Type
		{
			Ethernet,
		};

	public:
		NetworkInterface();
		virtual ~NetworkInterface() {}

		virtual BAN::MACAddress get_mac_address() const = 0;

		BAN::IPv4Address get_ipv4_address() const { return m_ipv4_address; }
		void set_ipv4_address(BAN::IPv4Address new_address) { m_ipv4_address = new_address; }

		BAN::IPv4Address get_netmask() const { return m_netmask; }
		void set_netmask(BAN::IPv4Address new_netmask) { m_netmask = new_netmask; }

		virtual bool link_up() = 0;
		virtual int link_speed() = 0;

		size_t interface_header_size() const;
		void add_interface_header(BAN::ByteSpan packet, BAN::MACAddress destination);

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const override { return m_name; }

		virtual BAN::ErrorOr<void> send_raw_bytes(BAN::ConstByteSpan) = 0;

	private:
		const Type m_type;

		const dev_t m_rdev;
		char m_name[10];

		BAN::IPv4Address m_ipv4_address { 0 };
		BAN::IPv4Address m_netmask { 0 };
	};

}
