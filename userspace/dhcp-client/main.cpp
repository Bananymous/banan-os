#include <BAN/Debug.h>
#include <BAN/Endianness.h>
#include <BAN/IPv4.h>
#include <BAN/MAC.h>
#include <BAN/Vector.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/socket.h>

#define DEBUG_DHCP 1

struct DHCPPacket
{
	uint8_t							op;
	uint8_t							htype	{ 0x01 };
	uint8_t							hlen	{ 0x06 };
	uint8_t							hops	{ 0x00 };
	BAN::NetworkEndian<uint32_t>	xid		{ 0x3903F326 };
	BAN::NetworkEndian<uint16_t>	secs	{ 0x0000 };
	BAN::NetworkEndian<uint16_t>	flags	{ 0x0000 };
	BAN::IPv4Address				ciaddr	{ 0 };
	BAN::IPv4Address				yiaddr	{ 0 };
	BAN::IPv4Address				siaddr	{ 0 };
	BAN::IPv4Address				giaddr	{ 0 };
	BAN::MACAddress					chaddr;
	uint8_t							padding[10] {};
	uint8_t							legacy[192] {};
	BAN::NetworkEndian<uint32_t>	magic_cookie	{ 0x63825363 };
	uint8_t 						options[0x100];
};
static_assert(offsetof(DHCPPacket, options) == 240);

enum DHCPType
{
	SubnetMask = 1,
	Router = 3,
	DomainNameServer = 6,
	RequestedIPv4Address= 50,
	DHCPMessageType = 53,
	ServerIdentifier = 54,
	ParameterRequestList = 55,
	End = 255,
};

enum DHCPMessageType
{
	INVALID = 0,
	DHCPDISCOVER = 1,
	DHCPOFFER = 2,
	DHCPREQUEST = 3,
	DHCPDECLINE = 4,
	DHCPACK = 5,
};

BAN::MACAddress get_mac_address(int socket)
{
	ifreq ifreq;
	if (ioctl(socket, SIOCGIFHWADDR, &ifreq) == -1)
	{
		perror("ioctl");
		exit(1);
	}

	BAN::MACAddress mac_address;
	memcpy(&mac_address, ifreq.ifr_ifru.ifru_hwaddr.sa_data, sizeof(mac_address));
	return mac_address;
}

void update_ipv4_info(int socket, BAN::IPv4Address address, BAN::IPv4Address subnet)
{
	{
		ifreq ifreq;
		auto& ifru_addr = *reinterpret_cast<sockaddr_in*>(&ifreq.ifr_ifru.ifru_addr);
		ifru_addr.sin_family = AF_INET;
		ifru_addr.sin_addr.s_addr = address.raw;
		if (ioctl(socket, SIOCSIFADDR, &ifreq) == -1)
		{
			perror("ioctl");
			exit(1);
		}
	}

	{
		ifreq ifreq;
		auto& ifru_netmask = *reinterpret_cast<sockaddr_in*>(&ifreq.ifr_ifru.ifru_netmask);
		ifru_netmask.sin_family = AF_INET;
		ifru_netmask.sin_addr.s_addr = netmask.raw;
		if (ioctl(socket, SIOCSIFNETMASK, &ifreq) == -1)
		{
			perror("ioctl");
			exit(1);
		}
	}
}

void send_dhcp_packet(int socket, const DHCPPacket& dhcp_packet, BAN::IPv4Address server_ipv4)
{
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(67);
	server_addr.sin_addr.s_addr = server_ipv4.raw;

	if (sendto(socket, &dhcp_packet, sizeof(DHCPPacket), 0, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("sendto");
		exit(1);
	}
}

void send_dhcp_discover(int socket, BAN::MACAddress mac_address)
{
	DHCPPacket dhcp_packet;
	dhcp_packet.op = 0x01;
	dhcp_packet.chaddr = mac_address;

	size_t idx = 0;

	dhcp_packet.options[idx++] = DHCPMessageType;
	dhcp_packet.options[idx++] = 0x01;
	dhcp_packet.options[idx++] = DHCPDISCOVER;

	dhcp_packet.options[idx++] = ParameterRequestList;
	dhcp_packet.options[idx++] = 0x03;
	dhcp_packet.options[idx++] = DomainNameServer;
	dhcp_packet.options[idx++] = SubnetMask;
	dhcp_packet.options[idx++] = Router;

	dhcp_packet.options[idx++] = 0xFF;

	send_dhcp_packet(socket, dhcp_packet, BAN::IPv4Address { 0xFFFFFFFF });
}

void send_dhcp_request(int socket, BAN::MACAddress mac_address, BAN::IPv4Address offered_ipv4, BAN::IPv4Address server_ipv4)
{
	DHCPPacket dhcp_packet;
	dhcp_packet.op = 0x01;
	dhcp_packet.siaddr = server_ipv4.raw;
	dhcp_packet.chaddr = mac_address;

	size_t idx = 0;

	dhcp_packet.options[idx++] = DHCPMessageType;
	dhcp_packet.options[idx++] = 0x01;
	dhcp_packet.options[idx++] = DHCPREQUEST;

	dhcp_packet.options[idx++] = RequestedIPv4Address;
	dhcp_packet.options[idx++] = 0x04;
	dhcp_packet.options[idx++] = offered_ipv4.octets[0];
	dhcp_packet.options[idx++] = offered_ipv4.octets[1];
	dhcp_packet.options[idx++] = offered_ipv4.octets[2];
	dhcp_packet.options[idx++] = offered_ipv4.octets[3];

	dhcp_packet.options[idx++] = 0xFF;

	send_dhcp_packet(socket, dhcp_packet, BAN::IPv4Address { 0xFFFFFFFF });
}

struct DHCPPacketInfo
{
	enum DHCPMessageType message_type	{ INVALID };
	BAN::IPv4Address address			{ 0 };
	BAN::IPv4Address subnet				{ 0 };
	BAN::IPv4Address server				{ 0 };
	BAN::Vector<BAN::IPv4Address> routers;
	BAN::Vector<BAN::IPv4Address> dns;
};

DHCPPacketInfo parse_dhcp_packet(const DHCPPacket& packet)
{
	DHCPPacketInfo packet_info;
	packet_info.address = BAN::IPv4Address(packet.yiaddr);

	const uint8_t* options = packet.options;
	while (*options != End)
	{
		uint8_t type = *options++;
		uint8_t length = *options++;

		switch (type)
		{
			case SubnetMask:
			{
				if (length != 4)
				{
					fprintf(stderr, "Subnet mask with invalid length %hhu\n", length);
					break;
				}
				uint32_t raw = *reinterpret_cast<const uint32_t*>(options);
				packet_info.subnet = BAN::IPv4Address(raw);
				break;
			}
			case Router:
			{
				if (length % 4 != 0)
				{
					fprintf(stderr, "Router with invalid length %hhu\n", length);
					break;
				}
				for (int i = 0; i < length; i += 4)
				{
					uint32_t raw = *reinterpret_cast<const uint32_t*>(options + i);
					MUST(packet_info.routers.emplace_back(raw));
				}
				break;
			}
			case DomainNameServer:
			{
				if (length % 4 != 0)
				{
					fprintf(stderr, "DNS with invalid length %hhu\n", length);
					break;
				}
				for (int i = 0; i < length; i += 4)
				{
					uint32_t raw = *reinterpret_cast<const uint32_t*>(options + i);
					MUST(packet_info.dns.emplace_back(raw));
				}
				break;
			}
			case DHCPMessageType:
			{
				if (length != 1)
				{
					fprintf(stderr, "DHCP Message Type with invalid length %hhu\n", length);
					break;
				}
				switch (*options)
				{
					case DHCPDISCOVER:	packet_info.message_type = DHCPDISCOVER;	break;
					case DHCPOFFER:		packet_info.message_type = DHCPOFFER;		break;
					case DHCPREQUEST:	packet_info.message_type = DHCPREQUEST;		break;
					case DHCPDECLINE:	packet_info.message_type = DHCPDECLINE;		break;
					case DHCPACK:		packet_info.message_type = DHCPACK;			break;
				}
				break;
			}
			case ServerIdentifier:
			{
				if (length != 4)
				{
					fprintf(stderr, "Server identifier with invalid length %hhu\n", length);
					break;
				}
				uint32_t raw = *reinterpret_cast<const uint32_t*>(options);
				packet_info.server = BAN::IPv4Address(raw);
				break;
			}
		}

		options += length;
	}

	return packet_info;
}

BAN::Optional<DHCPPacketInfo> read_dhcp_packet(int socket)
{
	DHCPPacket dhcp_packet;

	ssize_t nrecv = recvfrom(socket, &dhcp_packet, sizeof(dhcp_packet), 0, nullptr, nullptr);
	if (nrecv == -1)
	{
		perror("revcfrom");
		return {};
	}

	if (nrecv <= (ssize_t)offsetof(DHCPPacket, options))
	{
		fprintf(stderr, "invalid DHCP offer\n");
		return {};
	}

	if (dhcp_packet.magic_cookie != 0x63825363)
	{
		fprintf(stderr, "invalid DHCP offer\n");
		return {};
	}

	return parse_dhcp_packet(dhcp_packet);
}

int main()
{
	int socket = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (socket == -1)
	{
		perror("socket");
		return 1;
	}

	sockaddr_in client_addr;
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(68);
	client_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(socket, (sockaddr*)&client_addr, sizeof(client_addr)) == -1)
	{
		perror("bind");
		return 1;
	}

	auto mac_address = get_mac_address(socket);
#if DEBUG_DHCP
	dprintln("MAC: {}", mac_address);
#endif

	send_dhcp_discover(socket, mac_address);
#if DEBUG_DHCP
	dprintln("DHCPDISCOVER sent");
#endif

	auto dhcp_offer = read_dhcp_packet(socket);
	if (!dhcp_offer.has_value())
		return 1;
	if (dhcp_offer->message_type != DHCPOFFER)
	{
		fprintf(stderr, "DHCP server did not respond with DHCPOFFER\n");
		return 1;
	}

#if DEBUG_DHCP
	dprintln("DHCPOFFER");
	dprintln("  IP     {}", dhcp_offer->address);
	dprintln("  SUBNET {}", dhcp_offer->subnet);
	dprintln("  SERVER {}", dhcp_offer->server);
#endif

	send_dhcp_request(socket, mac_address, dhcp_offer->address, dhcp_offer->server);
#if DEBUG_DHCP
	dprintln("DHCPREQUEST sent");
#endif

	auto dhcp_ack = read_dhcp_packet(socket);
	if (!dhcp_ack.has_value())
		return 1;
	if (dhcp_ack->message_type != DHCPACK)
	{
		fprintf(stderr, "DHCP server did not respond with DHCPACK\n");
		return 1;
	}

#if DEBUG_DHCP
	dprintln("DHCPACK");
	dprintln("  IP     {}", dhcp_ack->address);
	dprintln("  SUBNET {}", dhcp_ack->subnet);
	dprintln("  SERVER {}", dhcp_ack->server);
#endif

	if (dhcp_offer->address != dhcp_ack->address)
	{
		fprintf(stderr, "DHCP server OFFER and ACK ips don't match\n");
		return 1;
	}

	update_ipv4_info(socket, dhcp_ack->address, dhcp_ack->subnet);

	close(socket);

	return 0;
}
