#include <kernel/Memory/Heap.h>
#include <kernel/Networking/UDPSocket.h>
#include <kernel/Thread.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<UDPSocket>> UDPSocket::create(mode_t mode, uid_t uid, gid_t gid)
	{
		auto socket = TRY(BAN::RefPtr<UDPSocket>::create(mode, uid, gid));
		socket->m_packet_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			packet_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		return socket;
	}

	UDPSocket::UDPSocket(mode_t mode, uid_t uid, gid_t gid)
		: NetworkSocket(mode, uid, gid)
	{ }

	void UDPSocket::add_protocol_header(BAN::ByteSpan packet, uint16_t src_port, uint16_t dst_port)
	{
		auto& header = packet.as<UDPHeader>();
		header.src_port = src_port;
		header.dst_port = dst_port;
		header.length = packet.size();
		header.checksum = 0;
	}

	void UDPSocket::add_packet(BAN::ConstByteSpan packet, BAN::IPv4Address sender_addr, uint16_t sender_port)
	{
		CriticalScope _;

		if (m_packets.full())
		{
			dprintln("Packet buffer full, dropping packet");
			return;
		}

		if (!m_packets.empty() && m_packet_total_size > m_packet_buffer->size())
		{
			dprintln("Packet buffer full, dropping packet");
			return;
		}

		void* buffer = reinterpret_cast<void*>(m_packet_buffer->vaddr() + m_packet_total_size);
		memcpy(buffer, packet.data(), packet.size());

		m_packets.push(PacketInfo {
			.sender_addr = sender_addr,
			.sender_port = sender_port,
			.packet_size = packet.size()
		});
		m_packet_total_size += packet.size();

		m_semaphore.unblock();
	}

	BAN::ErrorOr<size_t> UDPSocket::read_packet(BAN::ByteSpan buffer, sockaddr_in* sender_addr)
	{
		while (m_packets.empty())
			TRY(Thread::current().block_or_eintr(m_semaphore));

		CriticalScope _;
		if (m_packets.empty())
			return read_packet(buffer, sender_addr);

		auto packet_info = m_packets.front();
		m_packets.pop();

		size_t nread = BAN::Math::min<size_t>(packet_info.packet_size, buffer.size());

		memcpy(
			buffer.data(),
			(const void*)m_packet_buffer->vaddr(),
			nread
		);
		memmove(
			(void*)m_packet_buffer->vaddr(),
			(void*)(m_packet_buffer->vaddr() + packet_info.packet_size),
			m_packet_total_size - packet_info.packet_size
		);

		m_packet_total_size -= packet_info.packet_size;

		if (sender_addr)
		{
			sender_addr->sin_family = AF_INET;
			sender_addr->sin_port = packet_info.sender_port;
			sender_addr->sin_addr.s_addr = packet_info.sender_addr.as_u32();
		}

		return nread;
	}

}
