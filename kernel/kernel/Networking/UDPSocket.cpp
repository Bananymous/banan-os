#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Networking/UDPSocket.h>
#include <kernel/Thread.h>

#include <sys/epoll.h>
#include <sys/ioctl.h>

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
			true, false
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

	BAN::ErrorOr<void> UDPSocket::connect_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len > static_cast<socklen_t>(sizeof(m_peer_address)))
			address_len = sizeof(m_peer_address);
		memcpy(&m_peer_address, address, address_len);
		m_peer_address_len = address_len;
		return {};
	}

	BAN::ErrorOr<void> UDPSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (is_bound())
			return BAN::Error::from_errno(EINVAL);
		return m_network_layer.bind_socket_to_address(this, address, address_len);
	}

	BAN::ErrorOr<size_t> UDPSocket::recvmsg_impl(msghdr& message, int flags)
	{
		flags &= (MSG_OOB | MSG_PEEK | MSG_WAITALL);
		if (flags != 0)
		{
			dwarnln("TODO: recvmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (CMSG_FIRSTHDR(&message))
		{
			dwarnln("ignoring recvmsg control message");
			message.msg_controllen = 0;
		}

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

		auto* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		message.msg_flags = 0;

		size_t total_recv = 0;
		for (int i = 0; i < message.msg_iovlen; i++)
		{
			const size_t nrecv = BAN::Math::min<size_t>(message.msg_iov[i].iov_len, packet_info.packet_size - total_recv);
			memcpy(message.msg_iov[i].iov_base, packet_buffer + total_recv, nrecv);
			total_recv += nrecv;

			if (nrecv < packet_info.packet_size)
				message.msg_flags |= MSG_TRUNC;
		}

		memmove(
			packet_buffer,
			packet_buffer + packet_info.packet_size,
			m_packet_total_size - packet_info.packet_size
		);

		m_packet_total_size -= packet_info.packet_size;

		if (message.msg_name && message.msg_namelen)
		{
			const size_t namelen = BAN::Math::min<size_t>(message.msg_namelen, sizeof(sockaddr_storage));
			memcpy(message.msg_name, &packet_info.sender, namelen);
			message.msg_namelen = namelen;
		}

		return total_recv;
	}

	BAN::ErrorOr<size_t> UDPSocket::sendmsg_impl(const msghdr& message, int flags)
	{
		if (flags & MSG_NOSIGNAL)
			dwarnln("sendmsg ignoring MSG_NOSIGNAL");
		flags &= (MSG_EOR | MSG_OOB /* | MSG_NOSIGNAL */);
		if (flags != 0)
		{
			dwarnln("TODO: sendmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (CMSG_FIRSTHDR(&message))
			dwarnln("ignoring sendmsg control message");

		if (!is_bound())
			TRY(m_network_layer.bind_socket_to_unused(this, static_cast<sockaddr*>(message.msg_name), message.msg_namelen));

		const size_t total_send_size =
			[&message]() -> size_t {
				size_t result = 0;
				for (int i = 0; i < message.msg_iovlen; i++)
					result += message.msg_iov[i].iov_len;
				return result;
			}();

		BAN::Vector<uint8_t> buffer;
		TRY(buffer.resize(total_send_size));

		size_t offset = 0;
		for (int i = 0; i < message.msg_iovlen; i++)
		{
			memcpy(buffer.data() + offset, message.msg_iov[i].iov_base, message.msg_iov[i].iov_len);
			offset += message.msg_iov[i].iov_len;
		}

		sockaddr* address;
		socklen_t address_len;
		if (!message.msg_name || message.msg_namelen == 0)
		{
			if (m_peer_address_len == 0)
				return BAN::Error::from_errno(EDESTADDRREQ);
			address = reinterpret_cast<sockaddr*>(&m_peer_address);
			address_len = m_peer_address_len;
		}
		else
		{
			address = static_cast<sockaddr*>(message.msg_name);
			address_len = message.msg_namelen;
		}

		return TRY(m_network_layer.sendto(*this, buffer.span(), address, address_len));
	}

	BAN::ErrorOr<long> UDPSocket::ioctl_impl(int request, void* argument)
	{
		switch (request)
		{
			case FIONREAD:
			{
				SpinLockGuard guard(m_packet_lock);
				if (m_packets.empty())
					*static_cast<int*>(argument) = 0;
				else
					*static_cast<int*>(argument) = m_packets.front().packet_size;
				return 0;
			}
		}

		return NetworkSocket::ioctl_impl(request, argument);
	}

}
