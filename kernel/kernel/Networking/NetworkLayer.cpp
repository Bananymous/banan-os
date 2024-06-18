#include <kernel/Networking/NetworkLayer.h>

namespace Kernel
{

	uint16_t calculate_internet_checksum(BAN::ConstByteSpan packet, const PseudoHeader& pseudo_header)
	{
		uint32_t checksum = 0;
		for (size_t i = 0; i < sizeof(pseudo_header) / sizeof(uint16_t); i++)
			checksum += BAN::host_to_network_endian(reinterpret_cast<const uint16_t*>(&pseudo_header)[i]);
		for (size_t i = 0; i < packet.size() / sizeof(uint16_t); i++)
			checksum += BAN::host_to_network_endian(reinterpret_cast<const uint16_t*>(packet.data())[i]);
		if (packet.size() % 2)
			checksum += (uint16_t)packet[packet.size() - 1] << 8;
		while (checksum >> 16)
			checksum = (checksum >> 16) + (checksum & 0xFFFF);
		return ~(uint16_t)checksum;
	}

}
