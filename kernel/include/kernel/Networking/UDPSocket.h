#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/Endianness.h>
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

		virtual size_t protocol_header_size() const override { return sizeof(UDPHeader); }
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port) override;
		virtual NetworkProtocol protocol() const override { return NetworkProtocol::UDP; }

	protected:
		virtual void add_packet(BAN::ConstByteSpan, BAN::IPv4Address sender_addr, uint16_t sender_port) override;
		virtual BAN::ErrorOr<size_t> read_packet(BAN::ByteSpan, sockaddr_in* sender_address) override;

	private:
		UDPSocket(NetworkLayer&, ino_t, const TmpInodeInfo&);

		struct PacketInfo
		{
			BAN::IPv4Address	sender_addr;
			uint16_t			sender_port;
			size_t				packet_size;
		};

	private:
		static constexpr size_t				packet_buffer_size = 10 * PAGE_SIZE;
		BAN::UniqPtr<VirtualRange>			m_packet_buffer;
		BAN::CircularQueue<PacketInfo, 128>	m_packets;
		size_t								m_packet_total_size { 0 };
		Semaphore							m_semaphore;

		friend class BAN::RefPtr<UDPSocket>;
	};

}
