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

		virtual bool link_up() = 0;
		virtual int link_speed() = 0;

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const override { return m_name; }

	protected:
		virtual BAN::ErrorOr<void> send_raw_bytes(BAN::ConstByteSpan) = 0;

	private:
		const Type m_type;

		const dev_t m_rdev;
		char m_name[10];

		uint32_t m_ipv4_address {};
	};

}
