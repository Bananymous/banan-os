#pragma once

#include <BAN/Errors.h>
#include <BAN/ByteSpan.h>
#include <kernel/Device/Device.h>

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

		virtual uint8_t* get_mac_address() = 0;
		uint32_t get_ipv4_address() const { return m_ipv4_address; }

		virtual bool link_up() = 0;
		virtual int link_speed() = 0;

		BAN::ErrorOr<void> add_interface_header(BAN::Vector<uint8_t>&, uint8_t destination_mac[6]);

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const override { return m_name; }

		virtual BAN::ErrorOr<void> send_raw_bytes(BAN::ConstByteSpan) = 0;

	private:
		const Type m_type;

		const dev_t m_rdev;
		char m_name[10];

		uint32_t m_ipv4_address {};
	};

}
