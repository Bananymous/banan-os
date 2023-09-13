#pragma once

#include <BAN/Errors.h>

namespace Kernel
{

	class NetworkDriver
	{
	public:
		virtual ~NetworkDriver() {}

		virtual uint8_t* get_mac_address() = 0;
		virtual BAN::ErrorOr<void> send_packet(const void* data, uint16_t len) = 0;

		virtual bool link_up() = 0;
		virtual int link_speed() = 0;
	};

}