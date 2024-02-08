#include <BAN/HashMap.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/UNIX/Socket.h>
#include <kernel/Scheduler.h>

#include <fcntl.h>
#include <sys/un.h>

namespace Kernel
{

	static BAN::HashMap<BAN::String, BAN::RefPtr<UnixDomainSocket>>	s_bound_sockets;
	static SpinLock													s_bound_socket_lock;

	static constexpr size_t s_packet_buffer_size = 10 * PAGE_SIZE;

	BAN::ErrorOr<BAN::RefPtr<UnixDomainSocket>> UnixDomainSocket::create(SocketType socket_type, ino_t ino, const TmpInodeInfo& inode_info)
	{
		auto socket = TRY(BAN::RefPtr<UnixDomainSocket>::create(socket_type, ino, inode_info));
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

	UnixDomainSocket::UnixDomainSocket(SocketType socket_type, ino_t ino, const TmpInodeInfo& inode_info)
		: TmpInode(NetworkManager::get(), ino, inode_info)
		, m_socket_type(socket_type)
	{
		switch (socket_type)
		{
			case SocketType::STREAM:
			case SocketType::SEQPACKET:
				m_info.emplace<ConnectionInfo>();
				break;
			case SocketType::DGRAM:
				m_info.emplace<ConnectionlessInfo>();
				break;
			default:
				ASSERT_NOT_REACHED();
		}
	}

	BAN::ErrorOr<long> UnixDomainSocket::accept_impl(sockaddr* address, socklen_t* address_len)
	{
		if (!m_info.has<ConnectionInfo>())
			return BAN::Error::from_errno(EOPNOTSUPP);
		auto& connection_info = m_info.get<ConnectionInfo>();
		if (!connection_info.listening)
			return BAN::Error::from_errno(EINVAL);

		while (connection_info.pending_connections.empty())
			TRY(Thread::current().block_or_eintr(connection_info.pending_semaphore));

		BAN::RefPtr<UnixDomainSocket> pending;

		{
			LockGuard _(connection_info.pending_lock);
			pending = connection_info.pending_connections.front();
			connection_info.pending_connections.pop();
			connection_info.pending_semaphore.unblock();
		}

		BAN::RefPtr<UnixDomainSocket> return_inode;

		{
			auto return_inode_tmp = TRY(NetworkManager::get().create_socket(SocketDomain::UNIX, m_socket_type, mode().mode & ~Mode::TYPE_MASK, uid(), gid()));
			return_inode = reinterpret_cast<UnixDomainSocket*>(return_inode_tmp.ptr());
		}

		TRY(return_inode->m_bound_path.append(m_bound_path));
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

		return TRY(Process::current().open_inode(return_inode, O_RDWR));
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
			LockGuard _(s_bound_socket_lock);
			if (!s_bound_sockets.contains(file.canonical_path))
				return BAN::Error::from_errno(ECONNREFUSED);
			target = s_bound_sockets[file.canonical_path];
		}

		if (m_socket_type != target->m_socket_type)
			return BAN::Error::from_errno(EPROTOTYPE);

		if (m_info.has<ConnectionInfo>())
		{
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
					LockGuard _(target_info.pending_lock);
					if (target_info.pending_connections.size() < target_info.pending_connections.capacity())
					{
						MUST(target_info.pending_connections.push(this));
						target_info.pending_semaphore.unblock();
						break;
					}
				}
				TRY(Thread::current().block_or_eintr(target_info.pending_semaphore));
			}

			while (!connection_info.connection_done)
				Scheduler::get().reschedule();

			return {};
		}
		else
		{
			return BAN::Error::from_errno(ENOTSUP);
		}
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

		LockGuard _(s_bound_socket_lock);
		ASSERT(!s_bound_sockets.contains(file.canonical_path));
		TRY(s_bound_sockets.emplace(file.canonical_path, this));
		m_bound_path = BAN::move(file.canonical_path);

		return {};
	}

	bool UnixDomainSocket::is_streaming() const
	{
		switch (m_socket_type)
		{
			case SocketType::STREAM:
				return true;
			case SocketType::SEQPACKET:
			case SocketType::DGRAM:
				return false;
			default:
				ASSERT_NOT_REACHED();
		}
	}

	// This to feels too hacky to expose out of here
	struct LockFreeGuard
	{
		LockFreeGuard(RecursivePrioritySpinLock& lock)
			: m_lock(lock)
			, m_depth(lock.lock_depth())
		{
			for (uint32_t i = 0; i < m_depth; i++)
				m_lock.unlock();
		}

		~LockFreeGuard()
		{
			for (uint32_t i = 0; i < m_depth; i++)
				m_lock.lock();
		}

	private:
		RecursivePrioritySpinLock& m_lock;
		const uint32_t m_depth;
	};

	BAN::ErrorOr<void> UnixDomainSocket::add_packet(BAN::ConstByteSpan packet)
	{
		LockGuard _(m_lock);

		while (m_packet_sizes.full() || m_packet_size_total + packet.size() > s_packet_buffer_size)
		{
			LockFreeGuard _(m_lock);
			TRY(Thread::current().block_or_eintr(m_packet_semaphore));
		}

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr() + m_packet_size_total);
		memcpy(packet_buffer, packet.data(), packet.size());
		m_packet_size_total += packet.size();

		if (!is_streaming())
			m_packet_sizes.push(packet.size());

		m_packet_semaphore.unblock();
		return {};
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::sendto_impl(const sys_sendto_t* arguments)
	{
		if (arguments->flags)
			return BAN::Error::from_errno(ENOTSUP);
		if (arguments->length > s_packet_buffer_size)
			return BAN::Error::from_errno(ENOBUFS);

		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (arguments->dest_addr)
				return BAN::Error::from_errno(EISCONN);
			auto target = connection_info.connection.lock();
			if (!target)
				return BAN::Error::from_errno(ENOTCONN);
			TRY(target->add_packet({ reinterpret_cast<const uint8_t*>(arguments->message), arguments->length }));
			return arguments->length;
		}
		else
		{
			return BAN::Error::from_errno(ENOTSUP);
		}
	}

	BAN::ErrorOr<size_t> UnixDomainSocket::recvfrom_impl(sys_recvfrom_t* arguments)
	{
		if (arguments->flags)
			return BAN::Error::from_errno(ENOTSUP);

		if (m_info.has<ConnectionInfo>())
		{
			auto& connection_info = m_info.get<ConnectionInfo>();
			if (!connection_info.connection)
				return BAN::Error::from_errno(ENOTCONN);
		}

		while (m_packet_size_total == 0)
		{
			LockFreeGuard _(m_lock);
			TRY(Thread::current().block_or_eintr(m_packet_semaphore));
		}

		uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(m_packet_buffer->vaddr());

		size_t nread = 0;
		if (is_streaming())
			nread = BAN::Math::min(arguments->length, m_packet_size_total);
		else
		{
			nread = BAN::Math::min(arguments->length, m_packet_sizes.front());
			m_packet_sizes.pop();
		}

		memcpy(arguments->buffer, packet_buffer, nread);
		memmove(packet_buffer, packet_buffer + nread, m_packet_size_total - nread);
		m_packet_size_total -= nread;

		m_packet_semaphore.unblock();

		return nread;
	}

}
