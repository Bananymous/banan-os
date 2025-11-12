#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/Endianness.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	struct UDPHeader
	{
		BAN::NetworkEndian<uint16_t> src_port;
		BAN::NetworkEndian<uint16_t> dst_port;
		BAN::NetworkEndian<uint16_t> length;
		BAN::NetworkEndian<uint16_t> checksum;
	};
	static_assert(sizeof(UDPHeader) == 8);

	class UDPSocket final : public NetworkSocket
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UDPSocket>> create(NetworkLayer&, const Socket::Info&);

		virtual NetworkProtocol protocol() const override { return NetworkProtocol::UDP; }

		virtual size_t protocol_header_size() const override { return sizeof(UDPHeader); }
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) override;

	protected:
		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr* sender, socklen_t sender_len) override;

		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<size_t> recvmsg_impl(msghdr& message, int flags) override;
		virtual BAN::ErrorOr<size_t> sendmsg_impl(const msghdr& message, int flags) override;
		virtual BAN::ErrorOr<void> getpeername_impl(sockaddr*, socklen_t*) override { return BAN::Error::from_errno(ENOTCONN); }

		virtual BAN::ErrorOr<long> ioctl_impl(int, void*) override;

		virtual bool can_read_impl() const override { return !m_packets.empty(); }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override { return false; }

	private:
		UDPSocket(NetworkLayer&, const Socket::Info&);
		~UDPSocket();

		struct PacketInfo
		{
			sockaddr_storage	sender;
			size_t				packet_size;
		};

	private:
		static constexpr size_t				packet_buffer_size = 10 * PAGE_SIZE;
		BAN::UniqPtr<VirtualRange>			m_packet_buffer;
		BAN::CircularQueue<PacketInfo, 32>	m_packets;
		size_t								m_packet_total_size { 0 };
		SpinLock							m_packet_lock;
		ThreadBlocker						m_packet_thread_blocker;

		sockaddr_storage					m_peer_address {};
		socklen_t							m_peer_address_len { 0 };

		friend class BAN::RefPtr<UDPSocket>;
	};

}
