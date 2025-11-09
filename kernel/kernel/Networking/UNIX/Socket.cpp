#include <BAN/HashMap.h>

#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
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
	static Mutex s_bound_socket_lock;

	static constexpr size_t s_packet_buffer_size = 10 * PAGE_SIZE;

	static BAN::ErrorOr<BAN::StringView> validate_sockaddr_un(const sockaddr* address, socklen_t address_len)
	{
		if (address_len < static_cast<socklen_t>(sizeof(sa_family_t)))
			return BAN::Error::from_errno(EINVAL);
		if (address_len > static_cast<socklen_t>(sizeof(sockaddr_un)))
			address_len = sizeof(sockaddr_un);

		const auto& sockaddr_un = *reinterpret_cast<const struct sockaddr_un*>(address);
		if (sockaddr_un.sun_family != AF_UNIX)
			return BAN::Error::from_errno(EINVAL);

		size_t length = 0;
		while (length < address_len - sizeof(sa_family_t) && sockaddr_un.sun_path[length])
			length++;

		return BAN::StringView { sockaddr_un.sun_path, length };
	}

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
			LockGuard _(s_bound_socket_lock);
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
			LockGuard _(connection_info.pending_lock);
			while (connection_info.pending_connections.empty())
				TRY(Thread::current().block_or_eintr_indefinite(connection_info.pending_thread_blocker, &connection_info.pending_lock));

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
		const auto sun_path = TRY(validate_sockaddr_un(address, address_len));
		if (!is_bound())
			TRY(m_bound_file.canonical_path.push_back('X'));

		auto absolute_path = TRY(Process::current().absolute_path_of(sun_path));
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(
			Process::current().root_file().inode,
			Process::current().credentials(),
			absolute_path,
			O_RDWR
		));

		BAN::RefPtr<UnixDomainSocket> target;

		{
			LockGuard _(s_bound_socket_lock);
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

			LockGuard _(target_info.pending_lock);

			if (target_info.pending_connections.size() < target_info.pending_connections.capacity())
			{
				MUST(target_info.pending_connections.push(this));
				target_info.pending_thread_blocker.unblock();
				break;
			}

			TRY(Thread::current().block_or_eintr_indefinite(target_info.pending_thread_blocker, &target_info.pending_lock));
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

		const auto sun_path = TRY(validate_sockaddr_un(address, address_len));
		if (sun_path.empty())
			return BAN::Error::from_errno(EINVAL);

		// FIXME: This feels sketchy
		auto parent_file = sun_path.front() == '/'
			? TRY(Process::current().root_file().clone())
			: TRY(Process::current().working_directory().clone());
		if (auto ret = Process::current().create_file_or_dir(AT_FDCWD, sun_path.data(), 0755 | S_IFSOCK); ret.is_error())
		{
			if (ret.error().get_error_code() == EEXIST)
				return BAN::Error::from_errno(EADDRINUSE);
			return ret.release_error();
		}
		auto file = TRY(VirtualFileSystem::get().file_from_relative_path(
			Process::current().root_file().inode,
			parent_file,
			Process::current().credentials(),
			sun_path,
			O_RDWR
		));

		LockGuard _(s_bound_socket_lock);
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

	BAN::ErrorOr<void> UnixDomainSocket::add_packet(const msghdr& packet, size_t total_size, BAN::Vector<FDWrapper>&& fds_to_send)
	{
		LockGuard _(m_packet_lock);

		while (m_packet_infos.full() || m_packet_size_total + total_size > s_packet_buffer_size)
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &m_packet_lock));

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr() + m_packet_size_total);

		size_t offset = 0;
		for (int i = 0; i < packet.msg_iovlen; i++)
		{
			memcpy(packet_buffer + offset, packet.msg_iov[i].iov_base, packet.msg_iov[i].iov_len);
			offset += packet.msg_iov[i].iov_len;
		}

		ASSERT(offset == total_size);
		m_packet_size_total += total_size;
		m_packet_infos.emplace(total_size, BAN::move(fds_to_send));

		m_packet_thread_blocker.unblock();

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

	BAN::ErrorOr<size_t> UnixDomainSocket::recvmsg_impl(msghdr& message, int flags)
	{
		if (flags != 0)
		{
			dwarnln("TODO: recvmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		LockGuard _(m_packet_lock);
		while (m_packet_size_total == 0)
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

			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &m_packet_lock));
		}

		auto* cheader = CMSG_FIRSTHDR(&message);
		if (cheader != nullptr)
			cheader->cmsg_len = message.msg_controllen;
		size_t cheader_len = 0;

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		message.msg_flags = 0;

		size_t total_recv = 0;
		for (int i = 0; i < message.msg_iovlen; i++)
		{
			auto& packet_info = m_packet_infos.front();

			auto fds_to_open = BAN::move(packet_info.fds);
			if (cheader == nullptr && !fds_to_open.empty())
				message.msg_flags |= MSG_CTRUNC;
			else if (!fds_to_open.empty())
			{
				const size_t max_fd_count = (cheader->cmsg_len - sizeof(cmsghdr)) / sizeof(int);
				if (max_fd_count < fds_to_open.size())
					message.msg_flags |= MSG_CTRUNC;

				const size_t fd_count = BAN::Math::min(fds_to_open.size(), max_fd_count);
				const size_t fds_opened = Process::current().open_file_descriptor_set().open_all_fd_wrappers(fds_to_open.span().slice(0, fd_count));

				auto* fd_data = reinterpret_cast<int*>(CMSG_DATA(cheader));
				for (size_t i = 0; i < fds_opened; i++)
					fd_data[i] = fds_to_open[i].fd();

				const size_t header_length = CMSG_LEN(fds_opened * sizeof(int));
				cheader->cmsg_level = SOL_SOCKET;
				cheader->cmsg_type = SCM_RIGHTS;
				cheader->cmsg_len = header_length;
				cheader = CMSG_NXTHDR(&message, cheader);
				if (cheader != nullptr)
					cheader->cmsg_len = message.msg_controllen - header_length;
				cheader_len += header_length;
			}

			const size_t nrecv = BAN::Math::min<size_t>(message.msg_iov[i].iov_len, packet_info.size);
			memcpy(message.msg_iov[i].iov_base, packet_buffer, nrecv);
			total_recv += nrecv;

			if (!is_streaming() && nrecv < packet_info.size)
				message.msg_flags |= MSG_TRUNC;

			const size_t to_discard = is_streaming() ? nrecv : packet_info.size;

			packet_info.size -= to_discard;
			if (packet_info.size == 0)
				m_packet_infos.pop();

			// FIXME: get rid of this memmove :)
			memmove(packet_buffer, packet_buffer + to_discard, m_packet_size_total - to_discard);
			m_packet_size_total -= to_discard;

			if (!is_streaming() || m_packet_infos.empty())
				break;
		}

		message.msg_controllen = cheader_len;

		m_packet_thread_blocker.unblock();

		epoll_notify(EPOLLOUT);

		return total_recv;
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::sendmsg_impl(const msghdr& message, int flags)
	{
		if (flags != 0)
		{
			dwarnln("TODO: sendmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		BAN::Vector<OpenFileDescriptorSet::FDWrapper> fds_to_send;

		for (const auto* header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header))
		{
			if (header->cmsg_level != SOL_SOCKET)
			{
				dwarnln("ignoring control message with level {}", header->cmsg_level);
				continue;
			}

			if (header->cmsg_type != SCM_RIGHTS)
			{
				dwarnln("ignoring control message with type {}", header->cmsg_type);
				continue;
			}

			const auto* fd_data = reinterpret_cast<const int*>(CMSG_DATA(header));
			const size_t fd_count = (header->cmsg_len - sizeof(cmsghdr)) / sizeof(int);
			for (size_t i = 0; i < fd_count; i++)
				TRY(fds_to_send.push_back(TRY(Process::current().open_file_descriptor_set().get_fd_wrapper(fd_data[i]))));
		}

		const size_t total_message_size =
			[&message]() -> size_t {
				size_t result = 0;
				for (int i = 0; i < message.msg_iovlen; i++)
					result += message.msg_iov[i].iov_len;
				return result;
			}();

		if (total_message_size > s_packet_buffer_size)
			return BAN::Error::from_errno(ENOBUFS);

		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			auto target = connection_info.connection.lock();
			if (!target)
				return BAN::Error::from_errno(ENOTCONN);
			TRY(target->add_packet(message, total_message_size, BAN::move(fds_to_send)));
			return total_message_size;
		}
		else
		{
			BAN::RefPtr<Inode> target_inode;

			if (!message.msg_name)
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
				const auto sun_path = TRY(validate_sockaddr_un(static_cast<sockaddr*>(message.msg_name), message.msg_namelen));
				auto absolute_path = TRY(Process::current().absolute_path_of(sun_path));
				target_inode = TRY(VirtualFileSystem::get().file_from_absolute_path(
					Process::current().root_file().inode,
					Process::current().credentials(),
					absolute_path,
					O_WRONLY
				)).inode;
			}

			BAN::RefPtr<UnixDomainSocket> target;
			{
				LockGuard _(s_bound_socket_lock);
				auto it = s_bound_sockets.find(target_inode);
				if (it == s_bound_sockets.end())
					return BAN::Error::from_errno(EDESTADDRREQ);
				target = it->value.lock();
			}

			if (!target)
				return BAN::Error::from_errno(EDESTADDRREQ);
			TRY(target->add_packet(message, total_message_size, BAN::move(fds_to_send)));

			return total_message_size;
		}
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
