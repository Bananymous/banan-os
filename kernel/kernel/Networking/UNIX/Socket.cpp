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

	static constexpr size_t s_packet_buffer_size = 0x10000;

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
		, m_sndbuf(s_packet_buffer_size)
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
		}
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::add_packet(const msghdr& packet, PacketInfo&& packet_info, bool dont_block)
	{
		LockGuard _(m_packet_lock);

		const auto has_space =
			[&]() -> bool
			{
				if (m_packet_infos.full())
					return false;
				if (is_streaming())
					return m_packet_size_total < m_packet_buffer->size();
				return m_packet_size_total + packet_info.size <= m_packet_buffer->size();
			};

		while (!has_space())
		{
			if (dont_block)
				return BAN::Error::from_errno(EAGAIN);
			TRY(Thread::current().block_or_eintr_indefinite(m_packet_thread_blocker, &m_packet_lock));
		}

		if (auto available = m_packet_buffer->size() - m_packet_size_total; available < packet_info.size)
		{
			ASSERT(is_streaming());
			packet_info.size = available;
		}

		uint8_t* packet_buffer_base_u8 = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		size_t bytes_copied = 0;
		for (int i = 0; i < packet.msg_iovlen && bytes_copied < packet_info.size; i++)
		{
			const uint8_t* iov_base_u8 = static_cast<const uint8_t*>(packet.msg_iov[i].iov_base);

			const size_t to_copy = BAN::Math::min(packet.msg_iov[i].iov_len, packet_info.size - bytes_copied);

			const size_t copy_offset = (m_packet_buffer_tail + m_packet_size_total + bytes_copied) % m_packet_buffer->size();
			const size_t before_wrap = BAN::Math::min(to_copy, m_packet_buffer->size() - copy_offset);
			memcpy(packet_buffer_base_u8 + copy_offset, iov_base_u8, before_wrap);
			if (const size_t after_wrap = to_copy - before_wrap)
				memcpy(packet_buffer_base_u8, iov_base_u8 + before_wrap, after_wrap);

			bytes_copied += to_copy;
		}

		ASSERT(bytes_copied == packet_info.size);
		m_packet_size_total += packet_info.size;
		m_packet_infos.emplace(BAN::move(packet_info));

		m_packet_thread_blocker.unblock();

		epoll_notify(EPOLLIN);

		return bytes_copied;
	}

	bool UnixDomainSocket::can_read_impl() const
	{
		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (connection_info.listening)
				return !connection_info.pending_connections.empty();
			if (connection_info.target_closed)
				return true;
			if (!connection_info.connection)
				return false;
		}

		LockGuard _(m_packet_lock);
		return m_packet_size_total > 0;
	}

	bool UnixDomainSocket::can_write_impl() const
	{
		return m_bytes_sent < m_sndbuf;
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
		flags &= (MSG_OOB | MSG_PEEK | MSG_WAITALL);
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

		uint8_t* packet_buffer_base_u8 = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		message.msg_flags = 0;

		int iov_index = 0;
		size_t iov_offset = 0;
		size_t total_recv = 0;

		while (!m_packet_infos.empty() && iov_index < message.msg_iovlen)
		{
			auto& packet_info = m_packet_infos.front();

			auto fds_to_open = BAN::move(packet_info.fds);
			auto ucred_to_recv = BAN::move(packet_info.ucred);
			const bool had_ancillary_data = !fds_to_open.empty() || ucred_to_recv.has_value();

			if (!fds_to_open.empty()) do
			{
				if (cheader == nullptr)
				{
					dwarnln("no space to receive {} fds", fds_to_open.size());
					message.msg_flags |= MSG_CTRUNC;
					break;
				}

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
			} while (false);

			if (ucred_to_recv.has_value()) do
			{
				if (cheader == nullptr || cheader->cmsg_len - sizeof(cmsghdr) < sizeof(struct ucred))
				{
					dwarnln("no space to receive credentials");
					message.msg_flags |= MSG_CTRUNC;
					break;
				}

				*reinterpret_cast<struct ucred*>(CMSG_DATA(cheader)) = ucred_to_recv.value();

				const size_t header_length = CMSG_LEN(sizeof(struct ucred));
				cheader->cmsg_level = SOL_SOCKET;
				cheader->cmsg_type = SCM_CREDENTIALS;
				cheader->cmsg_len = header_length;
				cheader = CMSG_NXTHDR(&message, cheader);
				if (cheader != nullptr)
					cheader->cmsg_len = message.msg_controllen - header_length;
				cheader_len += header_length;
			} while (false);

			size_t packet_received = 0;
			while (iov_index < message.msg_iovlen && packet_received < packet_info.size)
			{
				auto& iov = message.msg_iov[iov_index];
				uint8_t* iov_base = static_cast<uint8_t*>(iov.iov_base);

				const size_t nrecv = BAN::Math::min<size_t>(iov.iov_len - iov_offset, packet_info.size - packet_received);

				const size_t copy_offset = (m_packet_buffer_tail + packet_received) % m_packet_buffer->size();
				const size_t before_wrap = BAN::Math::min<size_t>(nrecv, m_packet_buffer->size() - copy_offset);
				memcpy(iov_base + iov_offset, packet_buffer_base_u8 + copy_offset, before_wrap);
				if (const size_t after_wrap = nrecv - before_wrap)
					memcpy(iov_base + iov_offset + before_wrap, packet_buffer_base_u8, after_wrap);

				packet_received += nrecv;

				iov_offset += nrecv;
				if (iov_offset >= iov.iov_len)
				{
					iov_offset = 0;
					iov_index++;
				}
			}

			if (!is_streaming() && packet_received < packet_info.size)
				message.msg_flags |= MSG_TRUNC;

			const size_t to_discard = is_streaming() ? packet_received : packet_info.size;

			packet_info.size -= to_discard;
			if (packet_info.size == 0)
				m_packet_infos.pop();

			m_packet_buffer_tail = (m_packet_buffer_tail + to_discard) % m_packet_buffer->size();
			m_packet_size_total -= to_discard;

			total_recv += packet_received;

			if (auto sender = packet_info.sender.lock())
			{
				sender->m_bytes_sent -= to_discard;
				sender->epoll_notify(EPOLLOUT);
			}

			// on linux ancillary data is a barrier on stream sockets, lets do the same
			if (!is_streaming() || had_ancillary_data)
				break;
		}

		message.msg_controllen = cheader_len;

		m_packet_thread_blocker.unblock();

		return total_recv;
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::sendmsg_impl(const msghdr& message, int flags)
	{
		if (flags & ~(MSG_NOSIGNAL | MSG_DONTWAIT))
		{
			dwarnln("TODO: sendmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		const size_t total_message_size =
			[&message]() -> size_t {
				size_t result = 0;
				for (int i = 0; i < message.msg_iovlen; i++)
					result += message.msg_iov[i].iov_len;
				return result;
			}();

		if (!is_streaming() && total_message_size > m_packet_buffer->size())
			return BAN::Error::from_errno(EMSGSIZE);

		PacketInfo packet_info {
			.size   = total_message_size,
			.fds    = {},
			.ucred  = {},
			.sender = TRY(get_weak_ptr()),
		};

		for (const auto* header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header))
		{
			if (header->cmsg_level != SOL_SOCKET)
			{
				dwarnln("ignoring control message with level {}", header->cmsg_level);
				continue;
			}

			switch (header->cmsg_type)
			{
				case SCM_RIGHTS:
				{
					if (!packet_info.fds.empty())
					{
						dwarnln("multiple SCM_RIGHTS in one sendmsg");
						return BAN::Error::from_errno(EINVAL);
					}

					const auto* fd_data = reinterpret_cast<const int*>(CMSG_DATA(header));
					const size_t fd_count = (header->cmsg_len - sizeof(cmsghdr)) / sizeof(int);
					for (size_t i = 0; i < fd_count; i++)
						TRY(packet_info.fds.push_back(TRY(Process::current().open_file_descriptor_set().get_fd_wrapper(fd_data[i]))));
					break;
				}
				case SCM_CREDENTIALS:
				{
					if (packet_info.ucred.has_value())
					{
						dwarnln("multiple SCM_CREDENTIALS in one sendmsg");
						return BAN::Error::from_errno(EINVAL);
					}

					if (header->cmsg_len - sizeof(cmsghdr) < sizeof(struct ucred))
						return BAN::Error::from_errno(EINVAL);
					const ucred* ucred = reinterpret_cast<const struct ucred*>(CMSG_DATA(header));

					const bool is_valid_ucred =
						[ucred]() -> bool
						{
							const auto& creds = Process::current().credentials();
							if (creds.is_superuser())
								return true;
							if (ucred->pid != Process::current().pid())
								return false;
							if (ucred->uid != creds.ruid() && ucred->uid != creds.euid() && ucred->uid != creds.suid())
								return false;
							if (ucred->gid != creds.rgid() && !creds.has_egid(ucred->gid) && ucred->gid != creds.sgid())
								return false;
							return true;
						}();

					if (!is_valid_ucred)
						return BAN::Error::from_errno(EPERM);

					packet_info.ucred = *ucred;

					break;
				}
				default:
					dwarnln("ignoring control message with type {}", header->cmsg_type);
					break;
			}
		}

		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			auto target = connection_info.connection.lock();
			if (!target)
				return BAN::Error::from_errno(ENOTCONN);
			const size_t bytes_sent = TRY(target->add_packet(message, BAN::move(packet_info), flags & MSG_DONTWAIT));
			m_bytes_sent += bytes_sent;
			return bytes_sent;
		}
		else
		{
			BAN::RefPtr<Inode> target_inode;

			if (!message.msg_name || message.msg_namelen == 0)
			{
				auto& connectionless_info = m_info.get<ConnectionlessInfo>();
				if (connectionless_info.peer_address.empty())
					return BAN::Error::from_errno(EDESTADDRREQ);

				auto absolute_path = TRY(Process::current().absolute_path_of(connectionless_info.peer_address));
				target_inode = TRY(VirtualFileSystem::get().file_from_absolute_path(
					Process::current().root_file().inode,
					Process::current().credentials(),
					absolute_path,
					O_WRONLY
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
			if (target->m_socket_type != m_socket_type)
				return BAN::Error::from_errno(EPROTOTYPE);
			const auto bytes_sent = TRY(target->add_packet(message, BAN::move(packet_info), flags & MSG_DONTWAIT));
			m_bytes_sent += bytes_sent;
			return bytes_sent;
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

	BAN::ErrorOr<void> UnixDomainSocket::getsockopt_impl(int level, int option, void* value, socklen_t* value_len)
	{
		if (level != SOL_SOCKET)
			return BAN::Error::from_errno(EINVAL);

		int result;
		switch (option)
		{
			case SO_ERROR:
				result = 0;
				break;
			case SO_SNDBUF:
				result = m_sndbuf;
				break;
			case SO_RCVBUF:
				result = m_packet_buffer->size();
				break;
			default:
				dwarnln("getsockopt(SOL_SOCKET, {})", option);
				return BAN::Error::from_errno(ENOTSUP);
		}

		const size_t len = BAN::Math::min<size_t>(sizeof(result), *value_len);
		memcpy(value, &result, len);
		*value_len = sizeof(int);

		return {};
	}

	BAN::ErrorOr<void> UnixDomainSocket::setsockopt_impl(int level, int option, const void* value, socklen_t value_len)
	{
		if (level != SOL_SOCKET)
			return BAN::Error::from_errno(EINVAL);

		switch (option)
		{
			case SO_SNDBUF:
			{
				if (value_len != sizeof(int))
					return BAN::Error::from_errno(EINVAL);
				const int new_sndbuf = *static_cast<const int*>(value);
				if (new_sndbuf < 0)
					return BAN::Error::from_errno(EINVAL);
				m_sndbuf = new_sndbuf;
				break;
			}
			default:
				dwarnln("setsockopt(SOL_SOCKET, {})", option);
				return BAN::Error::from_errno(ENOTSUP);
		}

		return {};
	}

}
