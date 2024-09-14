#include <BAN/HashMap.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/UNIX/Socket.h>
#include <kernel/Scheduler.h>

#include <fcntl.h>
#include <sys/un.h>

namespace Kernel
{

	static BAN::HashMap<BAN::String, BAN::WeakPtr<UnixDomainSocket>>	s_bound_sockets;
	static SpinLock														s_bound_socket_lock;

	static constexpr size_t s_packet_buffer_size = 10 * PAGE_SIZE;

	BAN::ErrorOr<BAN::RefPtr<UnixDomainSocket>> UnixDomainSocket::create(Socket::Type socket_type, const Socket::Info& info)
	{
		auto socket = TRY(BAN::RefPtr<UnixDomainSocket>::create(socket_type, info));
		socket->m_packet_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			s_packet_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		return socket;
	}

	UnixDomainSocket::UnixDomainSocket(Socket::Type socket_type, const Socket::Info& info)
		: Socket(info)
		, m_socket_type(socket_type)
	{
		switch (socket_type)
		{
			case Socket::Type::STREAM:
			case Socket::Type::SEQPACKET:
				m_info.emplace<ConnectionInfo>();
				break;
			case Socket::Type::DGRAM:
				m_info.emplace<ConnectionlessInfo>();
				break;
			default:
				ASSERT_NOT_REACHED();
		}
	}

	UnixDomainSocket::~UnixDomainSocket()
	{
		if (is_bound() && !is_bound_to_unused())
		{
			SpinLockGuard _(s_bound_socket_lock);
			auto it = s_bound_sockets.find(m_bound_path);
			if (it != s_bound_sockets.end())
				s_bound_sockets.remove(it);
		}
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (auto connection = connection_info.connection.lock(); connection && connection->m_info.has<ConnectionInfo>())
				connection->m_info.get<ConnectionInfo>().target_closed = true;
		}
	}

	BAN::ErrorOr<long> UnixDomainSocket::accept_impl(sockaddr* address, socklen_t* address_len, int flags)
	{
		if (!m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(EOPNOTSUPP);
		auto& connection_info = m_info.get<ConnectionInfo>();
		if (!connection_info.listening)
			return BAN::Error::from_errno(EINVAL);

		while (connection_info.pending_connections.empty())
			TRY(Thread::current().block_or_eintr_indefinite(connection_info.pending_thread_blocker));

		BAN::RefPtr<UnixDomainSocket> pending;

		{
			SpinLockGuard _(connection_info.pending_lock);
			pending = connection_info.pending_connections.front();
			connection_info.pending_connections.pop();
			connection_info.pending_thread_blocker.unblock();
		}

		BAN::RefPtr<UnixDomainSocket> return_inode;

		{
			auto return_inode_tmp = TRY(NetworkManager::get().create_socket(Socket::Domain::UNIX, m_socket_type, mode().mode & ~Mode::TYPE_MASK, uid(), gid()));
			return_inode = reinterpret_cast<UnixDomainSocket*>(return_inode_tmp.ptr());
		}

		TRY(return_inode->m_bound_path.push_back('X'));
		return_inode->m_info.get<ConnectionInfo>().connection = TRY(pending->get_weak_ptr());
		pending->m_info.get<ConnectionInfo>().connection = TRY(return_inode->get_weak_ptr());
		pending->m_info.get<ConnectionInfo>().connection_done = true;

		if (address && address_len && !is_bound_to_unused())
		{
			size_t copy_len = BAN::Math::min<size_t>(*address_len, sizeof(sockaddr) + m_bound_path.size() + 1);
			auto& sockaddr_un = *reinterpret_cast<struct sockaddr_un*>(address);
			sockaddr_un.sun_family = AF_UNIX;
			strncpy(sockaddr_un.sun_path, pending->m_bound_path.data(), copy_len);
		}

		return TRY(Process::current().open_inode(VirtualFileSystem::File(return_inode, "<unix socket>"_sv), O_RDWR | flags));
	}

	BAN::ErrorOr<void> UnixDomainSocket::connect_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len != sizeof(sockaddr_un))
			return BAN::Error::from_errno(EINVAL);
		auto& sockaddr_un = *reinterpret_cast<const struct sockaddr_un*>(address);
		if (sockaddr_un.sun_family != AF_UNIX)
			return BAN::Error::from_errno(EAFNOSUPPORT);
		if (!is_bound())
			TRY(m_bound_path.push_back('X'));

		auto absolute_path = TRY(Process::current().absolute_path_of(sockaddr_un.sun_path));
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(
			Process::current().credentials(),
			absolute_path,
			O_RDWR
		));

		BAN::RefPtr<UnixDomainSocket> target;

		{
			SpinLockGuard _(s_bound_socket_lock);
			auto it = s_bound_sockets.find(file.canonical_path);
			if (it == s_bound_sockets.end())
				return BAN::Error::from_errno(ECONNREFUSED);
			target = it->value.lock();
			if (!target)
				return BAN::Error::from_errno(ECONNREFUSED);
		}

		if (m_socket_type != target->m_socket_type)
			return BAN::Error::from_errno(EPROTOTYPE);

		if (m_info.has<ConnectionlessInfo>())
		{
			auto& connectionless_info = m_info.get<ConnectionlessInfo>();
			connectionless_info.peer_address = BAN::move(file.canonical_path);
			return {};
		}

		auto& connection_info = m_info.get<ConnectionInfo>();
		if (connection_info.connection)
			return BAN::Error::from_errno(ECONNREFUSED);
		if (connection_info.listening)
			return BAN::Error::from_errno(EOPNOTSUPP);

		connection_info.connection_done = false;

		for (;;)
		{
			auto& target_info = target->m_info.get<ConnectionInfo>();
			{
				SpinLockGuard _(target_info.pending_lock);
				if (target_info.pending_connections.size() < target_info.pending_connections.capacity())
				{
					MUST(target_info.pending_connections.push(this));
					target_info.pending_thread_blocker.unblock();
					break;
				}
			}
			TRY(Thread::current().block_or_eintr_indefinite(target_info.pending_thread_blocker));
		}

		while (!connection_info.connection_done)
			Processor::yield();

		return {};
	}

	BAN::ErrorOr<void> UnixDomainSocket::listen_impl(int backlog)
	{
		backlog = BAN::Math::clamp(backlog, 1, SOMAXCONN);
		if (!is_bound())
			return BAN::Error::from_errno(EDESTADDRREQ);
		if (!m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(EOPNOTSUPP);
		auto& connection_info = m_info.get<ConnectionInfo>();
		if (connection_info.connection)
			return BAN::Error::from_errno(EINVAL);
		TRY(connection_info.pending_connections.reserve(backlog));
		connection_info.listening = true;
		return {};
	}

	BAN::ErrorOr<void> UnixDomainSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (is_bound())
			return BAN::Error::from_errno(EINVAL);
		if (address_len != sizeof(sockaddr_un))
			return BAN::Error::from_errno(EINVAL);
		auto& sockaddr_un = *reinterpret_cast<const struct sockaddr_un*>(address);
		if (sockaddr_un.sun_family != AF_UNIX)
			return BAN::Error::from_errno(EAFNOSUPPORT);

		auto absolute_path = TRY(Process::current().absolute_path_of(sockaddr_un.sun_path));
		if (auto ret = Process::current().create_file_or_dir(absolute_path, 0755 | S_IFSOCK); ret.is_error())
		{
			if (ret.error().get_error_code() == EEXIST)
				return BAN::Error::from_errno(EADDRINUSE);
			return ret.release_error();
		}
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(
			Process::current().credentials(),
			absolute_path,
			O_RDWR
		));

		SpinLockGuard _(s_bound_socket_lock);
		ASSERT(!s_bound_sockets.contains(file.canonical_path));
		TRY(s_bound_sockets.emplace(file.canonical_path, TRY(get_weak_ptr())));
		m_bound_path = BAN::move(file.canonical_path);

		return {};
	}

	bool UnixDomainSocket::is_streaming() const
	{
		switch (m_socket_type)
		{
			case Socket::Type::STREAM:
				return true;
			case Socket::Type::SEQPACKET:
			case Socket::Type::DGRAM:
				return false;
			default:
				ASSERT_NOT_REACHED();
		}
	}

	BAN::ErrorOr<void> UnixDomainSocket::add_packet(BAN::ConstByteSpan packet)
	{
		auto state = m_packet_lock.lock();
		while (m_packet_sizes.full() || m_packet_size_total + packet.size() > s_packet_buffer_size)
		{
			m_packet_lock.unlock(state);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker));
			state = m_packet_lock.lock();
		}

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr() + m_packet_size_total);
		memcpy(packet_buffer, packet.data(), packet.size());
		m_packet_size_total += packet.size();

		if (!is_streaming())
			m_packet_sizes.push(packet.size());

		m_packet_thread_blocker.unblock();
		m_packet_lock.unlock(state);
		return {};
	}

	bool UnixDomainSocket::can_read_impl() const
	{
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (connection_info.target_closed)
				return true;
			if (!connection_info.pending_connections.empty())
				return true;
			if (!connection_info.connection)
				return false;
		}

		return m_packet_size_total > 0;
	}

	bool UnixDomainSocket::can_write_impl() const
	{
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			return connection_info.connection.valid();
		}

		return true;
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::sendto_impl(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len)
	{
		if (message.size() > s_packet_buffer_size)
			return BAN::Error::from_errno(ENOBUFS);

		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (address)
				return BAN::Error::from_errno(EISCONN);
			auto target = connection_info.connection.lock();
			if (!target)
				return BAN::Error::from_errno(ENOTCONN);
			TRY(target->add_packet(message));
			return message.size();
		}
		else
		{
			BAN::String canonical_path;

			if (!address)
			{
				auto& connectionless_info = m_info.get<ConnectionlessInfo>();
				if (connectionless_info.peer_address.empty())
					return BAN::Error::from_errno(EDESTADDRREQ);
				TRY(canonical_path.append(connectionless_info.peer_address));
			}
			else
			{
				if (address_len != sizeof(sockaddr_un))
					return BAN::Error::from_errno(EINVAL);
				auto& sockaddr_un = *reinterpret_cast<const struct sockaddr_un*>(address);
				if (sockaddr_un.sun_family != AF_UNIX)
					return BAN::Error::from_errno(EAFNOSUPPORT);

				auto absolute_path = TRY(Process::current().absolute_path_of(sockaddr_un.sun_path));
				auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(
					Process::current().credentials(),
					absolute_path,
					O_WRONLY
				));

				canonical_path = BAN::move(file.canonical_path);
			}

			SpinLockGuard _(s_bound_socket_lock);
			auto it = s_bound_sockets.find(canonical_path);
			if (it == s_bound_sockets.end())
				return BAN::Error::from_errno(EDESTADDRREQ);
			auto target = it->value.lock();
			if (!target)
				return BAN::Error::from_errno(EDESTADDRREQ);
			TRY(target->add_packet(message));
			return message.size();
		}
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::recvfrom_impl(BAN::ByteSpan buffer, sockaddr*, socklen_t*)
	{
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			bool expected = true;
			if (connection_info.target_closed.compare_exchange(expected, false))
				return 0;
			if (!connection_info.connection)
				return BAN::Error::from_errno(ENOTCONN);
		}

		auto state = m_packet_lock.lock();
		while (m_packet_size_total == 0)
		{
			m_packet_lock.unlock(state);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker));
			state = m_packet_lock.lock();
		}

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		size_t nread = 0;
		if (is_streaming())
			nread = BAN::Math::min(buffer.size(), m_packet_size_total);
		else
		{
			nread = BAN::Math::min(buffer.size(), m_packet_sizes.front());
			m_packet_sizes.pop();
		}

		memcpy(buffer.data(), packet_buffer, nread);
		memmove(packet_buffer, packet_buffer + nread, m_packet_size_total - nread);
		m_packet_size_total -= nread;

		m_packet_thread_blocker.unblock();
		m_packet_lock.unlock(state);

		return nread;
	}

}
