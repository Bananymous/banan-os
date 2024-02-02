#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Endianness.h>
#include <BAN/IPv4.h>
#include <BAN/Vector.h>

namespace Kernel
{

	struct IPv4Header
	{
		uint8_t version_IHL;
		uint8_t DSCP_ECN;
		BAN::NetworkEndian<uint16_t> total_length	{ 0 };
		BAN::NetworkEndian<uint16_t> identification	{ 0 };
		BAN::NetworkEndian<uint16_t> flags_frament	{ 0 };
		uint8_t time_to_live;
		uint8_t protocol;
		BAN::NetworkEndian<uint16_t> checksum		{ 0 };
		BAN::IPv4Address src_address;
		BAN::IPv4Address dst_address;

		constexpr uint16_t calculate_checksum() const
		{
			return 0xFFFF
				- (((uint16_t)version_IHL << 8) | DSCP_ECN)
				- total_length
				- identification
				- flags_frament
				- (((uint16_t)time_to_live << 8) | protocol);
		}
	};
	static_assert(sizeof(IPv4Header) == 20);

	void add_ipv4_header(BAN::ByteSpan packet, BAN::IPv4Address src_ipv4, BAN::IPv4Address dst_ipv4, uint8_t protocol);

}
