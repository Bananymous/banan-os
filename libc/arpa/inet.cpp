#include <BAN/Endianness.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>

uint32_t htonl(uint32_t hostlong)
{
	return BAN::host_to_network_endian(hostlong);
}

uint16_t htons(uint16_t hostshort)
{
	return BAN::host_to_network_endian(hostshort);
}

uint32_t ntohl(uint32_t netlong)
{
	return BAN::host_to_network_endian(netlong);
}

uint16_t ntohs(uint16_t netshort)
{
	return BAN::host_to_network_endian(netshort);
}

in_addr_t inet_addr(const char* cp)
{
	uint32_t a = 0, b = 0, c = 0, d = 0;
	int ret = sscanf(cp, "%u.%u.%u.%u", &a, &b, &c, &d);
	if (ret < 1 || ret > 4)
		return (in_addr_t)(-1);
	uint32_t result = 0;
	result |= (ret == 1) ? a : a << 24;
	result |= (ret == 2) ? b : b << 16;
	result |= (ret == 3) ? c : c <<  8;
	result |= (ret == 4) ? d : d <<  0;
	return htonl(result);
}

char* inet_ntoa(struct in_addr in)
{
	static char buffer[16];
	uint32_t he = ntohl(in.s_addr);
	sprintf(buffer, "%u.%u.%u.%u",
		(he >> 24) & 0xFF,
		(he >> 16) & 0xFF,
		(he >>  8) & 0xFF,
		(he >>  0) & 0xFF
	);
	return buffer;
}
