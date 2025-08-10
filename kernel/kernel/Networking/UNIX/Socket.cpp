#include <BAN/HashMap.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/UNIX/Socket.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/un.h>

namespace Kernel
{

	struct UnixSocketHash
	{
		BAN::hash_t operator()(const BAN::RefPtr<Inode>& socket)
		{
			return BAN::hash<const Inode*>{}(socket.ptr());
		}
	};

	static BAN::HashMap<BAN::RefPtr<Inode>, BAN::WeakPtr<UnixDomainSocket>, UnixSocketHash> s_bound_sockets;
	static SpinLock s_bound_socket_lock;

	static constexpr size_t s_packet_buffer_size = 10 * PAGE_SIZE;

	// FIXME: why is this using spinlocks instead of mutexes??

	BAN::ErrorOr<BAN::RefPtr<UnixDomainSocket>> UnixDomainSocket::create(Socket::Type socket_type, const Socket::Info& info)
	{
		auto socket = TRY(BAN::RefPtr<UnixDomainSocket>::create(socket_type, info));
		socket->m_packet_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			s_packet_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, false
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
			s_bound_sockets.remove(m_bound_file.inode);
		}
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (auto connection = connection_info.connection.lock(); connection && connection->m_info.has<ConnectionInfo>())
			{
				connection->m_info.get<ConnectionInfo>().target_closed = true;
				connection->epoll_notify(EPOLLHUP);
				connection->m_packet_thread_blocker.unblock();
			}
		}
	}

	BAN::ErrorOr<void> UnixDomainSocket::make_socket_pair(UnixDomainSocket& other)
	{
		if (!m_info.has<ConnectionInfo>() || !other.m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(EINVAL);

		TRY(this->get_weak_ptr());
		TRY(other.get_weak_ptr());

		this->m_info.get<ConnectionInfo>().connection = MUST(other.get_weak_ptr());
		other.m_info.get<ConnectionInfo>().connection = MUST(this->get_weak_ptr());

		return {};
	}

	BAN::ErrorOr<long> UnixDomainSocket::accept_impl(sockaddr* address, socklen_t* address_len, int flags)
	{
		if (!m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(EOPNOTSUPP);
		auto& connection_info = m_info.get<ConnectionInfo>();
		if (!connection_info.listening)
			return BAN::Error::from_errno(EINVAL);


		BAN::RefPtr<UnixDomainSocket> pending;

		{
			SpinLockGuard guard(connection_info.pending_lock);

			SpinLockGuardAsMutex smutex(guard);
			while (connection_info.pending_connections.empty())
				TRY(Thread::current().block_or_eintr_indefinite(connection_info.pending_thread_blocker, &smutex));

			pending = connection_info.pending_connections.front();
			connection_info.pending_connections.pop();
			connection_info.pending_thread_blocker.unblock();
		}

		BAN::RefPtr<UnixDomainSocket> return_inode;

		{
			auto return_inode_tmp = TRY(NetworkManager::get().create_socket(Socket::Domain::UNIX, m_socket_type, mode().mode & ~Mode::TYPE_MASK, uid(), gid()));
			return_inode = reinterpret_cast<UnixDomainSocket*>(return_inode_tmp.ptr());
		}

		TRY(return_inode->m_bound_file.canonical_path.push_back('X'));
		return_inode->m_info.get<ConnectionInfo>().connection = TRY(pending->get_weak_ptr());
		pending->m_info.get<ConnectionInfo>().connection = TRY(return_inode->get_weak_ptr());
		pending->m_info.get<ConnectionInfo>().connection_done = true;

		if (address && address_len && !is_bound_to_unused())
		{
			sockaddr_un sa_un {
				.sun_family = AF_UNIX,
				.sun_path {},
			};
			strcpy(sa_un.sun_path, pending->m_bound_file.canonical_path.data());

			const size_t to_copy = BAN::Math::min<size_t>(*address_len, sizeof(sockaddr_un));
			memcpy(address, &sa_un, to_copy);
			*address_len = to_copy;
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
			TRY(m_bound_file.canonical_path.push_back('X'));

		auto absolute_path = TRY(Process::current().absolute_path_of(sockaddr_un.sun_path));
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(
			Process::current().root_file().inode,
			Process::current().credentials(),
			absolute_path,
			O_RDWR
		));

		BAN::RefPtr<UnixDomainSocket> target;

		{
			SpinLockGuard _(s_bound_socket_lock);
			auto it = s_bound_sockets.find(file.inode);
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

			SpinLockGuard guard(target_info.pending_lock);

			if (target_info.pending_connections.size() < target_info.pending_connections.capacity())
			{
				MUST(target_info.pending_connections.push(this));
				target_info.pending_thread_blocker.unblock();
				break;
			}

			SpinLockGuardAsMutex smutex(guard);
			TRY(Thread::current().block_or_eintr_indefinite(target_info.pending_thread_blocker, &smutex));
		}

		target->epoll_notify(EPOLLIN);

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

		auto bind_path = BAN::StringView(sockaddr_un.sun_path);
		if (bind_path.empty())
			return BAN::Error::from_errno(EINVAL);

		// FIXME: This feels sketchy
		auto parent_file = bind_path.front() == '/'
			? TRY(Process::current().root_file().clone())
			: TRY(Process::current().working_directory().clone());
		if (auto ret = Process::current().create_file_or_dir(AT_FDCWD, bind_path.data(), 0755 | S_IFSOCK); ret.is_error())
		{
			if (ret.error().get_error_code() == EEXIST)
				return BAN::Error::from_errno(EADDRINUSE);
			return ret.release_error();
		}
		auto file = TRY(VirtualFileSystem::get().file_from_relative_path(
			Process::current().root_file().inode,
			parent_file,
			Process::current().credentials(),
			bind_path,
			O_RDWR
		));

		SpinLockGuard _(s_bound_socket_lock);
		if (s_bound_sockets.contains(file.inode))
			return BAN::Error::from_errno(EADDRINUSE);
		TRY(s_bound_sockets.emplace(file.inode, TRY(get_weak_ptr())));
		m_bound_file = BAN::move(file);

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
			SpinLockAsMutex smutex(m_packet_lock, state);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &smutex));
		}

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr() + m_packet_size_total);
		memcpy(packet_buffer, packet.data(), packet.size());
		m_packet_size_total += packet.size();

		if (!is_streaming())
			m_packet_sizes.push(packet.size());

		m_packet_thread_blocker.unblock();
		m_packet_lock.unlock(state);

		epoll_notify(EPOLLIN);

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

	bool UnixDomainSocket::has_hungup_impl() const
	{
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			return connection_info.target_closed;
		}

		return false;
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
			BAN::RefPtr<Inode> target_inode;

			if (!address)
			{
				auto& connectionless_info = m_info.get<ConnectionlessInfo>();
				if (connectionless_info.peer_address.empty())
					return BAN::Error::from_errno(EDESTADDRREQ);

				auto absolute_path = TRY(Process::current().absolute_path_of(connectionless_info.peer_address));
				target_inode = TRY(VirtualFileSystem::get().file_from_absolute_path(
					Process::current().root_file().inode,
					Process::current().credentials(),
					absolute_path,
					O_RDWR
				)).inode;
			}
			else
			{
				if (address_len != sizeof(sockaddr_un))
					return BAN::Error::from_errno(EINVAL);
				auto& sockaddr_un = *reinterpret_cast<const struct sockaddr_un*>(address);
				if (sockaddr_un.sun_family != AF_UNIX)
					return BAN::Error::from_errno(EAFNOSUPPORT);

				auto absolute_path = TRY(Process::current().absolute_path_of(sockaddr_un.sun_path));
				target_inode = TRY(VirtualFileSystem::get().file_from_absolute_path(
					Process::current().root_file().inode,
					Process::current().credentials(),
					absolute_path,
					O_WRONLY
				)).inode;
			}

			SpinLockGuard _(s_bound_socket_lock);
			auto it = s_bound_sockets.find(target_inode);
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
		auto state = m_packet_lock.lock();
		while (m_packet_size_total == 0)
		{
			if (m_info.has<ConnectionInfo>())
			{
				auto& connection_info = m_info.get<ConnectionInfo>();
				bool expected = true;
				if (connection_info.target_closed.compare_exchange(expected, false))
				{
					m_packet_lock.unlock(state);
					return 0;
				}
				if (!connection_info.connection)
				{
					m_packet_lock.unlock(state);
					return BAN::Error::from_errno(ENOTCONN);
				}
			}

			SpinLockAsMutex smutex(m_packet_lock, state);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &smutex));
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

		epoll_notify(EPOLLOUT);

		return nread;
	}

	BAN::ErrorOr<void> UnixDomainSocket::getpeername_impl(sockaddr* address, socklen_t* address_len)
	{
		if (!m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(ENOTCONN);
		auto connection = m_info.get<ConnectionInfo>().connection.lock();
		if (!connection)
			return BAN::Error::from_errno(ENOTCONN);

		sockaddr_un sa_un {
			.sun_family = AF_UNIX,
			.sun_path = {},
		};
		strcpy(sa_un.sun_path, connection->m_bound_file.canonical_path.data());

		const size_t to_copy = BAN::Math::min<socklen_t>(sizeof(sockaddr_un), *address_len);
		memcpy(address, &sa_un, to_copy);
		*address_len = to_copy;
		return {};
	}

}
