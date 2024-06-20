#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/Endianness.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/Semaphore.h>

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
		static BAN::ErrorOr<BAN::RefPtr<UDPSocket>> create(NetworkLayer&, ino_t, const TmpInodeInfo&);

		virtual NetworkProtocol protocol() const override { return NetworkProtocol::UDP; }

		virtual size_t protocol_header_size() const override { return sizeof(UDPHeader); }
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) override;

	protected:
		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr* sender, socklen_t sender_len) override;

		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<size_t> sendto_impl(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<size_t> recvfrom_impl(BAN::ByteSpan buffer, sockaddr* address, socklen_t* address_len) override;

		virtual bool can_read_impl() const override { return !m_packets.empty(); }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }

	private:
		UDPSocket(NetworkLayer&, ino_t, const TmpInodeInfo&);
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
		Semaphore							m_packet_semaphore;

		friend class BAN::RefPtr<UDPSocket>;
	};

}
