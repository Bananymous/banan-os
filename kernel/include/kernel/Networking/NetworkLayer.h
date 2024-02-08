#pragma once

#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	struct PseudoHeader
	{
		BAN::IPv4Address src_ipv4				{ 0 };
		BAN::IPv4Address dst_ipv4				{ 0 };
		BAN::NetworkEndian<uint16_t> protocol	{ 0 };
		BAN::NetworkEndian<uint16_t> extra		{ 0 };
	};
	static_assert(sizeof(PseudoHeader) == 12);

	class NetworkSocket;
	enum class SocketType;

	class NetworkLayer
	{
	public:
		virtual ~NetworkLayer() {}

		virtual void unbind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) = 0;
		virtual BAN::ErrorOr<void> bind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) = 0;

		virtual BAN::ErrorOr<size_t> sendto(NetworkSocket&, BAN::ConstByteSpan, const sockaddr*, socklen_t) = 0;

	protected:
		NetworkLayer() = default;
	};

	static uint16_t calculate_internet_checksum(BAN::ConstByteSpan packet, const PseudoHeader& pseudo_header)
	{
		uint32_t checksum = 0;
		for (size_t i = 0; i < sizeof(pseudo_header) / sizeof(uint16_t); i++)
			checksum += BAN::host_to_network_endian(reinterpret_cast<const uint16_t*>(&pseudo_header)[i]);
		for (size_t i = 0; i < packet.size() / sizeof(uint16_t); i++)
			checksum += BAN::host_to_network_endian(reinterpret_cast<const uint16_t*>(packet.data())[i]);
		while (checksum >> 16)
			checksum = (checksum >> 16) + (checksum & 0xFFFF);
		return ~(uint16_t)checksum;
	}

}
