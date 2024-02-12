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

		virtual void unbind_socket(BAN::RefPtr<NetworkSocket>, uint16_t port) = 0;
		virtual BAN::ErrorOr<void> bind_socket_to_unused(BAN::RefPtr<NetworkSocket>, const sockaddr* send_address, socklen_t send_address_len) = 0;
		virtual BAN::ErrorOr<void> bind_socket_to_address(BAN::RefPtr<NetworkSocket>, const sockaddr* address, socklen_t address_len) = 0;

		virtual BAN::ErrorOr<size_t> sendto(NetworkSocket&, BAN::ConstByteSpan, const sockaddr*, socklen_t) = 0;

		virtual size_t header_size() const = 0;

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
		if (packet.size() % 2)
			checksum += (uint16_t)packet[packet.size() - 1] << 8;
		while (checksum >> 16)
			checksum = (checksum >> 16) + (checksum & 0xFFFF);
		return ~(uint16_t)checksum;
	}

}
