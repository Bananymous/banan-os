#include <kernel/Networking/NetworkLayer.h>

namespace Kernel
{

	uint16_t calculate_internet_checksum(BAN::ConstByteSpan buffer)
	{
		return calculate_internet_checksum({ &buffer, 1 });
	}

	uint16_t calculate_internet_checksum(BAN::Span<const BAN::ConstByteSpan> buffers)
	{
		uint32_t checksum = 0;

		for (size_t i = 0; i < buffers.size(); i++)
		{
			auto buffer = buffers[i];

			const uint16_t* buffer_u16 = reinterpret_cast<const uint16_t*>(buffer.data());
			for (size_t j = 0; j < buffer.size() / 2; j++)
				checksum += BAN::host_to_network_endian(buffer_u16[j]);
			if (buffer.size() % 2 == 0)
				continue;
			ASSERT(i == buffers.size() - 1);
			checksum += buffer[buffer.size() - 1] << 8;
		}

		while (checksum >> 16)
			checksum = (checksum >> 16) + (checksum & 0xFFFF);
		return ~(uint16_t)checksum;
	}

}
