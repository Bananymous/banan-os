#pragma once

#include <BAN/Endianness.h>
#include <stdint.h>

namespace Kernel
{

	struct ICMPHeader
	{
		uint8_t type;
		uint8_t code;
		BAN::NetworkEndian<uint16_t> checksum;
		BAN::NetworkEndian<uint32_t> rest;
	};
	static_assert(sizeof(ICMPHeader) == 8);

	enum ICMPType : uint8_t
	{
		EchoReply = 0x00,
		DestinationUnreachable = 0x03,
		EchoRequest = 0x08,
	};

}
