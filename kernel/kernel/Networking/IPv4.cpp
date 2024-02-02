#include <BAN/Endianness.h>
#include <kernel/Networking/IPv4.h>

namespace Kernel
{


	struct IPv4Header
	{
		uint8_t version_IHL;
		uint8_t DSCP_ECN;
		BAN::NetworkEndian<uint16_t> total_length;
		BAN::NetworkEndian<uint16_t> identification;
		BAN::NetworkEndian<uint16_t> flags_frament;
		uint8_t time_to_live;
		uint8_t protocol;
		BAN::NetworkEndian<uint16_t> header_checksum;
		BAN::NetworkEndian<uint32_t> src_address;
		BAN::NetworkEndian<uint32_t> dst_address;

		uint16_t checksum() const
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

	BAN::ErrorOr<void> add_ipv4_header(BAN::Vector<uint8_t>& packet, uint32_t src_ipv4, uint32_t dst_ipv4, uint8_t protocol)
	{
		TRY(packet.resize(packet.size() + sizeof(IPv4Header)));
		memmove(packet.data() + sizeof(IPv4Header), packet.data(), packet.size() - sizeof(IPv4Header));

		auto* header = reinterpret_cast<IPv4Header*>(packet.data());
		header->version_IHL		= 0x45;
		header->DSCP_ECN		= 0x10;
		header->total_length	= packet.size();
		header->identification	= 1;
		header->flags_frament	= 0x00;
		header->time_to_live	= 0x40;
		header->protocol		= protocol;
		header->header_checksum = header->checksum();
		header->src_address		= src_ipv4;
		header->dst_address		= dst_ipv4;

		return {};
	}

}
