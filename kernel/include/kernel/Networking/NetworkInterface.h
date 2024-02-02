#pragma once

#include <BAN/Errors.h>
#include <BAN/ByteSpan.h>
#include <BAN/MAC.h>
#include <kernel/Device/Device.h>
#include <kernel/Networking/IPv4.h>

namespace Kernel
{

	class NetworkInterface : public CharacterDevice
	{
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
	};

}
