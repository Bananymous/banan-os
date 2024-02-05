#include <BAN/Endianness.h>
#include <kernel/Networking/IPv4.h>

namespace Kernel
{

	void add_ipv4_header(BAN::ByteSpan packet, BAN::IPv4Address src_ipv4, BAN::IPv4Address dst_ipv4, uint8_t protocol)
	{
		auto& header = packet.as<IPv4Header>();
		header.version_IHL		= 0x45;
		header.DSCP_ECN			= 0x00;
		header.total_length		= packet.size();
		header.identification	= 1;
		header.flags_frament	= 0x00;
		header.time_to_live		= 0x40;
		header.protocol			= protocol;
		header.src_address		= src_ipv4;
		header.dst_address		= dst_ipv4;
		header.checksum			= header.calculate_checksum();
	}

}
