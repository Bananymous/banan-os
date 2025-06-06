#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Networking/UDPSocket.h>
#include <kernel/Thread.h>

#include <sys/epoll.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<UDPSocket>> UDPSocket::create(NetworkLayer& network_layer, const Socket::Info& info)
	{
		auto socket = TRY(BAN::RefPtr<UDPSocket>::create(network_layer, info));
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

	UDPSocket::UDPSocket(NetworkLayer& network_layer, const Socket::Info& info)
		: NetworkSocket(network_layer, info)
	{ }

	UDPSocket::~UDPSocket()
	{
		if (is_bound())
			m_network_layer.unbind_socket(m_port);
		m_port = PORT_NONE;
		m_interface = nullptr;
	}

	void UDPSocket::add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader)
	{
		auto& header = packet.as<UDPHeader>();
		header.src_port = m_port;
		header.dst_port = dst_port;
		header.length = packet.size();
		header.checksum = 0;
	}

	void UDPSocket::receive_packet(BAN::ConstByteSpan packet, const sockaddr* sender, socklen_t sender_len)
	{
		(void)sender_len;

		//auto& header = packet.as<const UDPHeader>();
		auto payload = packet.slice(sizeof(UDPHeader));

		SpinLockGuard _(m_packet_lock);

		if (m_packets.full())
		{
			dprintln("Packet buffer full, dropping packet");
			return;
		}

		if (m_packet_total_size + payload.size() > m_packet_buffer->size())
		{
			dprintln("Packet buffer full, dropping packet");
			return;
		}

		void* buffer = reinterpret_cast<void*>(m_packet_buffer->vaddr() + m_packet_total_size);
		memcpy(buffer, payload.data(), payload.size());

		PacketInfo packet_info;
		memcpy(&packet_info.sender, sender, sender_len);
		packet_info.packet_size = payload.size();
		m_packets.emplace(packet_info);
		m_packet_total_size += payload.size();

		epoll_notify(EPOLLIN);

		m_packet_thread_blocker.unblock();
	}

	BAN::ErrorOr<void> UDPSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (is_bound())
			return BAN::Error::from_errno(EINVAL);
		return m_network_layer.bind_socket_to_address(this, address, address_len);
	}

	BAN::ErrorOr<size_t> UDPSocket::recvfrom_impl(BAN::ByteSpan buffer, sockaddr* address, socklen_t* address_len)
	{
		if (!is_bound())
		{
			dprintln("No interface bound");
			return BAN::Error::from_errno(EINVAL);
		}
		ASSERT(m_port != PORT_NONE);

		SpinLockGuard guard(m_packet_lock);

		while (m_packets.empty())
		{
			SpinLockGuardAsMutex smutex(guard);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &smutex));
		}

		auto packet_info = m_packets.front();
		m_packets.pop();

		size_t nread = BAN::Math::min<size_t>(packet_info.packet_size, buffer.size());

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());
		memcpy(
			buffer.data(),
			packet_buffer,
			nread
		);
		memmove(
			packet_buffer,
			packet_buffer + packet_info.packet_size,
			m_packet_total_size - packet_info.packet_size
		);

		m_packet_total_size -= packet_info.packet_size;

		if (address && address_len)
		{
			if (*address_len > (socklen_t)sizeof(sockaddr_storage))
				*address_len = sizeof(sockaddr_storage);
			memcpy(address, &packet_info.sender, *address_len);
		}

		return nread;
	}

	BAN::ErrorOr<size_t> UDPSocket::sendto_impl(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len)
	{
		if (!is_bound())
			TRY(m_network_layer.bind_socket_to_unused(this, address, address_len));
		return TRY(m_network_layer.sendto(*this, message, address, address_len));
	}

}
