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
			uint32_t total_sum = 0;
			for (size_t i = 0; i < sizeof(IPv4Header) / sizeof(uint16_t); i++)
				total_sum += reinterpret_cast<const BAN::NetworkEndian<uint16_t>*>(this)[i];
			total_sum -= checksum;
			while (total_sum >> 16)
				total_sum = (total_sum >> 16) + (total_sum & 0xFFFF);
			return ~(uint16_t)total_sum;
		}
	};
	static_assert(sizeof(IPv4Header) == 20);

	void add_ipv4_header(BAN::ByteSpan packet, BAN::IPv4Address src_ipv4, BAN::IPv4Address dst_ipv4, uint8_t protocol);

}
