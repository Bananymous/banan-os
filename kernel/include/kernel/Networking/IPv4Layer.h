#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/CircularQueue.h>
#include <BAN/Endianness.h>
#include <BAN/IPv4.h>
#include <BAN/NoCopyMove.h>
#include <BAN/UniqPtr.h>
#include <kernel/Networking/ARPTable.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkLayer.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/Thread.h>

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
	};
	static_assert(sizeof(IPv4Header) == 20);

	class IPv4Layer final : public NetworkLayer
	{
		BAN_NON_COPYABLE(IPv4Layer);
		BAN_NON_MOVABLE(IPv4Layer);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<IPv4Layer>> create();

		ARPTable& arp_table() { return *m_arp_table; }

		BAN::ErrorOr<void> handle_ipv4_packet(NetworkInterface&, BAN::ConstByteSpan);

		virtual void unbind_socket(uint16_t port) override;
		virtual BAN::ErrorOr<void> bind_socket_with_target(BAN::RefPtr<NetworkSocket>, const sockaddr* target_address, socklen_t target_address_len) override;
		virtual BAN::ErrorOr<void> bind_socket_to_address(BAN::RefPtr<NetworkSocket>, const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<void> get_socket_address(BAN::RefPtr<NetworkSocket>, sockaddr* address, socklen_t* address_len) override;

		virtual BAN::ErrorOr<size_t> sendto(NetworkSocket&, BAN::ConstByteSpan, const sockaddr*, socklen_t) override;

		virtual Socket::Domain domain() const override { return Socket::Domain::INET ;}
		virtual size_t header_size() const override { return sizeof(IPv4Header); }

	private:
		IPv4Layer() = default;

		BAN::ErrorOr<in_port_t> find_free_port();

	private:
		BAN::UniqPtr<ARPTable> m_arp_table;

		RecursiveSpinLock m_bound_socket_lock;
		BAN::HashMap<int, BAN::WeakPtr<NetworkSocket>> m_bound_sockets;

		friend class BAN::UniqPtr<IPv4Layer>;
	};

}
