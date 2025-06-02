#include <BAN/Endianness.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
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
	in_addr addr;
	if (inet_aton(cp, &addr) == 0)
		return INADDR_NONE;
	return addr.s_addr;
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

int inet_aton(const char* cp, struct in_addr* inp)
{
	uint32_t a, b, c, d, n;
	const int ret = sscanf(cp, "%i%n.%i%n.%i%n.%i%n",
		&a, &n, &b, &n, &c, &n, &d, &n
	);

	if (ret < 1 || ret > 4 || cp[n] != '\0')
		return 0;
	if (ret == 1 && (a > 0xFFFFFFFF))
		return 0;
	if (ret == 2 && (a > 0xFF || b > 0xFFFFFF))
		return 0;
	if (ret == 3 && (a > 0xFF || b > 0xFF || c > 0xFFFF))
		return 0;
	if (ret == 4 && (a > 0xFF || b > 0xFF || c > 0xFF || d > 0xFF))
		return 0;

	uint32_t result = 0;
	result |= (ret == 1) ? a : a << 24;
	result |= (ret == 2) ? b : b << 16;
	result |= (ret == 3) ? c : c <<  8;
	result |= (ret == 4) ? d : d <<  0;
	inp->s_addr = htonl(result);
	return 1;
}

const char* inet_ntop(int af, const void* __restrict src, char* __restrict dst, socklen_t size)
{
	if (af == AF_INET)
	{
		if (size < INET_ADDRSTRLEN)
		{
			errno = ENOSPC;
			return nullptr;
		}
		uint32_t he = ntohl(reinterpret_cast<const in_addr*>(src)->s_addr);
		sprintf(dst, "%u.%u.%u.%u",
			(he >> 24) & 0xFF,
			(he >> 16) & 0xFF,
			(he >>  8) & 0xFF,
			(he >>  0) & 0xFF
		);
		return dst;
	}

	errno = EAFNOSUPPORT;
	return nullptr;
}
