#include <kernel/FS/Pipe.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/OpenFileDescriptorSet.h>
#include <kernel/Process.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>

namespace Kernel
{

	OpenFileDescriptorSet::OpenFileDescriptorSet(const Credentials& credentials)
		: m_credentials(credentials)
	{

	}

	OpenFileDescriptorSet::~OpenFileDescriptorSet()
	{
		close_all();
	}

	OpenFileDescriptorSet& OpenFileDescriptorSet::operator=(OpenFileDescriptorSet&& other)
	{
		LockGuard _(m_mutex);
		for (size_t i = 0; i < m_open_files.size(); i++)
			m_open_files[i] = BAN::move(other.m_open_files[i]);
		return *this;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::clone_from(const OpenFileDescriptorSet& other)
	{
		LockGuard _(m_mutex);

		close_all();

		for (int fd = 0; fd < (int)other.m_open_files.size(); fd++)
		{
			if (other.validate_fd(fd).is_error())
				continue;

			auto& open_file = m_open_files[fd];
			open_file.description      = other.m_open_files[fd].description;
			open_file.descriptor_flags = other.m_open_files[fd].descriptor_flags;
			open_file.inode()->on_clone(open_file.status_flags());
		}

		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::open(VirtualFileSystem::File&& file, int flags)
	{
		ASSERT(file.inode);

		if (flags & ~(O_ACCMODE | O_NOFOLLOW | O_APPEND | O_TRUNC | O_CLOEXEC | O_TTY_INIT | O_NOCTTY | O_DIRECTORY | O_CREAT | O_EXCL | O_NONBLOCK))
			return BAN::Error::from_errno(ENOTSUP);

		if ((flags & O_ACCMODE) != O_RDWR && __builtin_popcount(flags & O_ACCMODE) != 1)
			return BAN::Error::from_errno(EINVAL);

		if ((flags & O_DIRECTORY) && !file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		if ((flags & O_TRUNC) && (flags & O_WRONLY) && file.inode->mode().ifreg())
			TRY(file.inode->truncate(0));

		LockGuard _(m_mutex);
		constexpr int status_mask = O_APPEND | O_DSYNC | O_NONBLOCK | O_RSYNC | O_SYNC | O_ACCMODE;
		int fd = TRY(get_free_fd());
		m_open_files[fd].description = TRY(BAN::RefPtr<OpenFileDescription>::create(BAN::move(file), 0, flags & status_mask));
		m_open_files[fd].descriptor_flags = flags & O_CLOEXEC;
		return fd;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::open(BAN::StringView absolute_path, int flags)
	{
		return open(TRY(VirtualFileSystem::get().file_from_absolute_path(Process::current().root_file().inode, m_credentials, absolute_path, flags)), flags);
	}

	struct SocketInfo
	{
		Socket::Domain domain;
		Socket::Type type;
		int status_flags;
		int descriptor_flags;
	};

	static BAN::ErrorOr<SocketInfo> parse_socket_info(int domain, int type, int protocol)
	{
		SocketInfo info;

		bool valid_protocol = true;
		switch (domain)
		{
			case AF_INET:
				info.domain = Socket::Domain::INET;
				break;
			case AF_INET6:
				info.domain = Socket::Domain::INET6;
				break;
			case AF_UNIX:
				info.domain = Socket::Domain::UNIX;
				valid_protocol = false;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		info.status_flags = 0;
		info.descriptor_flags = 0;
		if (type & SOCK_NONBLOCK)
			info.status_flags |= O_NONBLOCK;
		if (type & SOCK_CLOEXEC)
			info.descriptor_flags |= O_CLOEXEC;
		type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

		switch (type)
		{
			case SOCK_STREAM:
				info.type = Socket::Type::STREAM;
				if (protocol != IPPROTO_TCP)
					valid_protocol = false;
				break;
			case SOCK_DGRAM:
				info.type = Socket::Type::DGRAM;
				if (protocol != IPPROTO_UDP)
					valid_protocol = false;
				break;
			case SOCK_SEQPACKET:
				info.type = Socket::Type::SEQPACKET;
				valid_protocol = false;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		if (protocol && !valid_protocol)
			return BAN::Error::from_errno(EPROTONOSUPPORT);

		return info;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::socket(int domain, int type, int protocol)
	{
		auto sock_info = TRY(parse_socket_info(domain, type, protocol));
		auto socket = TRY(NetworkManager::get().create_socket(sock_info.domain, sock_info.type, 0777, m_credentials.euid(), m_credentials.egid()));

		auto socket_sv = "<socket>"_sv;
		if (sock_info.domain == Socket::Domain::UNIX)
			socket_sv = "<unix socket>"_sv;
		else if (sock_info.type == Socket::Type::STREAM)
			socket_sv = "<tcp socket>";
		else if (sock_info.type == Socket::Type::DGRAM)
			socket_sv = "<udp socket>";

		LockGuard _(m_mutex);
		int fd = TRY(get_free_fd());
		m_open_files[fd].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(socket, socket_sv), 0, O_RDWR | sock_info.status_flags));
		m_open_files[fd].descriptor_flags = sock_info.descriptor_flags;
		return fd;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::socketpair(int domain, int type, int protocol, int socket_vector[2])
	{
		auto sock_info = TRY(parse_socket_info(domain, type, protocol));

		auto socket1 = TRY(NetworkManager::get().create_socket(sock_info.domain, sock_info.type, 0600, m_credentials.euid(), m_credentials.egid()));
		auto socket2 = TRY(NetworkManager::get().create_socket(sock_info.domain, sock_info.type, 0600, m_credentials.euid(), m_credentials.egid()));
		TRY(NetworkManager::get().connect_sockets(sock_info.domain, socket1, socket2));

		LockGuard _(m_mutex);

		TRY(get_free_fd_pair(socket_vector));
		m_open_files[socket_vector[0]].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(socket1, "<socketpair>"_sv), 0, O_RDWR | sock_info.status_flags));
		m_open_files[socket_vector[0]].descriptor_flags = sock_info.descriptor_flags;
		m_open_files[socket_vector[1]].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(socket2, "<socketpair>"_sv), 0, O_RDWR | sock_info.status_flags));
		m_open_files[socket_vector[1]].descriptor_flags = sock_info.descriptor_flags;
		return {};
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::pipe(int fds[2])
	{
		LockGuard _(m_mutex);

		TRY(get_free_fd_pair(fds));

		auto pipe = TRY(Pipe::create(m_credentials));
		m_open_files[fds[0]].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(pipe, "<pipe rd>"_sv), 0, O_RDONLY));
		m_open_files[fds[0]].descriptor_flags = 0;
		m_open_files[fds[1]].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(pipe, "<pipe wr>"_sv), 0, O_WRONLY));
		m_open_files[fds[1]].descriptor_flags = 0;

		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::dup2(int fildes, int fildes2)
	{
		if (fildes2 < 0 || fildes2 >= (int)m_open_files.size())
			return BAN::Error::from_errno(EBADF);

		LockGuard _(m_mutex);

		TRY(validate_fd(fildes));
		if (fildes == fildes2)
			return fildes;

		(void)close(fildes2);

		auto& open_file = m_open_files[fildes2];
		open_file.description = m_open_files[fildes].description;
		open_file.descriptor_flags = 0;
		open_file.inode()->on_clone(open_file.status_flags());

		return fildes2;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::fcntl(int fd, int cmd, uintptr_t extra)
	{
		LockGuard _(m_mutex);

		TRY(validate_fd(fd));

		switch (cmd)
		{
			case F_DUPFD:
			case F_DUPFD_CLOEXEC:
			{
				const int new_fd = TRY(get_free_fd());

				auto& open_file = m_open_files[new_fd];
				open_file.description = m_open_files[fd].description;
				open_file.descriptor_flags = (cmd == F_DUPFD_CLOEXEC) ? O_CLOEXEC : 0;
				open_file.inode()->on_clone(open_file.status_flags());

				return new_fd;
			}
			case F_GETFD:
				return m_open_files[fd].descriptor_flags;
			case F_SETFD:
				if (extra & FD_CLOEXEC)
					m_open_files[fd].descriptor_flags |= O_CLOEXEC;
				else
					m_open_files[fd].descriptor_flags &= ~O_CLOEXEC;
				return 0;
			case F_GETFL:
				return m_open_files[fd].status_flags();
			case F_SETFL:
				extra &= O_APPEND | O_DSYNC | O_NONBLOCK | O_RSYNC | O_SYNC;
				m_open_files[fd].status_flags() &= O_ACCMODE;
				m_open_files[fd].status_flags() |= extra;
				return 0;
			case F_GETLK:
			{
				dwarnln("TODO: proper fcntl F_GETLK");

				auto* param = reinterpret_cast<struct flock*>(extra);
				const auto& flock = m_open_files[fd].description->flock;

				if (flock.lockers.empty())
					param->l_type = F_UNLCK;
				else
				{
					*param = {
						.l_type = static_cast<short>(flock.shared ? F_RDLCK : F_WRLCK),
						.l_whence = SEEK_SET,
						.l_start = 0,
						.l_len = 1,
						.l_pid = *flock.lockers.begin(),
					};
				}

				return 0;
			}
			case F_SETLK:
			case F_SETLKW:
			{
				dwarnln("TODO: proper fcntl F_SETLK(W)");

				int op = cmd == F_SETLKW ? LOCK_NB : 0;
				switch (reinterpret_cast<const struct flock*>(extra)->l_type)
				{
					case F_UNLCK: op |= LOCK_UN; break;
					case F_RDLCK: op |= LOCK_SH; break;
					case F_WRLCK: op |= LOCK_EX; break;
					default:
						return BAN::Error::from_errno(EINVAL);
				}
				TRY(flock(fd, op));

				return 0;
			}
			default:
				break;
		}

		dwarnln("TODO: fcntl command {}", cmd);
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<off_t> OpenFileDescriptorSet::seek(int fd, off_t offset, int whence)
	{
		LockGuard _(m_mutex);

		TRY(validate_fd(fd));

		off_t base_offset;
		switch (whence)
		{
			case SEEK_SET:
				base_offset = 0;
				break;
			case SEEK_CUR:
				base_offset = m_open_files[fd].offset();
				break;
			case SEEK_END:
				base_offset = m_open_files[fd].inode()->size();
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		const off_t new_offset = base_offset + offset;
		if (new_offset < 0)
			return BAN::Error::from_errno(EINVAL);

		m_open_files[fd].offset() = new_offset;

		return new_offset;
	}

	BAN::ErrorOr<off_t> OpenFileDescriptorSet::tell(int fd) const
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return m_open_files[fd].offset();
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::truncate(int fd, off_t length)
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return m_open_files[fd].inode()->truncate(length);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::close(int fd)
	{
		LockGuard _(m_mutex);

		TRY(validate_fd(fd));

		auto& open_file = m_open_files[fd];

		if (auto& flock = open_file.description->flock; Thread::current().has_process() && flock.lockers.contains(Process::current().pid()))
		{
			flock.lockers.remove(Process::current().pid());
			if (flock.lockers.empty())
			{
				flock.locked = false;
				flock.thread_blocker.unblock();
			}
		}

		open_file.inode()->on_close(open_file.status_flags());
		open_file.description.clear();
		open_file.descriptor_flags = 0;

		return {};
	}

	void OpenFileDescriptorSet::close_all()
	{
		LockGuard _(m_mutex);
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
			(void)close(fd);
	}

	void OpenFileDescriptorSet::close_cloexec()
	{
		LockGuard _(m_mutex);
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (validate_fd(fd).is_error())
				continue;
			if (m_open_files[fd].descriptor_flags & O_CLOEXEC)
				(void)close(fd);
		}
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::flock(int fd, int op)
	{
		const auto pid = Process::current().pid();

		LockGuard _(m_mutex);

		for (;;)
		{
			TRY(validate_fd(fd));

			auto& flock = m_open_files[fd].description->flock;
			switch (op & ~LOCK_NB)
			{
				case LOCK_UN:
					flock.lockers.remove(pid);
					if (flock.lockers.empty())
					{
						flock.locked = false;
						flock.thread_blocker.unblock();
					}
					return {};
				case LOCK_SH:
					if (!flock.locked)
					{
						TRY(flock.lockers.insert(pid));
						flock.locked = true;
						flock.shared = true;
						return {};
					}
					if (flock.shared)
					{
						TRY(flock.lockers.insert(pid));
						return {};
					}
					break;
				case LOCK_EX:
					if (!flock.locked)
					{
						TRY(flock.lockers.insert(pid));
						flock.locked = true;
						flock.shared = false;
						return {};
					}
					if (flock.lockers.contains(pid))
						return {};
					break;
				default:
					return BAN::Error::from_errno(EINVAL);
			}

			if (op & LOCK_NB)
				return BAN::Error::from_errno(EWOULDBLOCK);

			TRY(Thread::current().block_or_eintr_indefinite(flock.thread_blocker, &m_mutex));
		}
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read(int fd, BAN::ByteSpan buffer)
	{
		BAN::RefPtr<Inode> inode;
		bool is_nonblock;
		off_t offset;

		{
			LockGuard _(m_mutex);
			TRY(validate_fd(fd));
			auto& open_file = m_open_files[fd];
			if (!(open_file.status_flags() & O_RDONLY))
				return BAN::Error::from_errno(EBADF);
			inode = open_file.inode();
			is_nonblock = !!(open_file.status_flags() & O_NONBLOCK);
			offset = open_file.offset();
		}

		if (inode->mode().ifsock())
		{
			iovec iov {
				.iov_base = buffer.data(),
				.iov_len = buffer.size(),
			};

			msghdr message {
				.msg_name = nullptr,
				.msg_namelen = 0,
				.msg_iov = &iov,
				.msg_iovlen = 1,
				.msg_control = nullptr,
				.msg_controllen = 0,
				.msg_flags = 0,
			};

			return recvmsg(fd, message, 0);
		}

		size_t nread;
		{
			LockGuard _(inode->m_mutex);
			if (!inode->can_read() && inode->has_hungup())
				return 0;
			if (is_nonblock && !inode->can_read())
				return BAN::Error::from_errno(EAGAIN);
			nread = TRY(inode->read(offset, buffer));
		}

		LockGuard _(m_mutex);
		// NOTE: race condition with offset, its UB per POSIX
		if (!validate_fd(fd).is_error())
			m_open_files[fd].offset() = offset + nread;
		return nread;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::write(int fd, BAN::ConstByteSpan buffer)
	{
		BAN::RefPtr<Inode> inode;
		bool is_nonblock;
		off_t offset;

		{
			LockGuard _(m_mutex);
			TRY(validate_fd(fd));
			auto& open_file = m_open_files[fd];
			if (!(open_file.status_flags() & O_WRONLY))
				return BAN::Error::from_errno(EBADF);
			inode = open_file.inode();
			is_nonblock = !!(open_file.status_flags() & O_NONBLOCK);
			offset = (open_file.status_flags() & O_APPEND) ? inode->size() : open_file.offset();
		}

		if (inode->mode().ifsock())
		{
			iovec iov {
				.iov_base = const_cast<uint8_t*>(buffer.data()),
				.iov_len = buffer.size(),
			};

			msghdr message {
				.msg_name = nullptr,
				.msg_namelen = 0,
				.msg_iov = &iov,
				.msg_iovlen = 1,
				.msg_control = nullptr,
				.msg_controllen = 0,
				.msg_flags = 0,
			};

			return sendmsg(fd, message, 0);
		}

		size_t nwrite;
		{
			LockGuard _(inode->m_mutex);
			if (inode->has_error())
			{
				Thread::current().add_signal(SIGPIPE, {});
				return BAN::Error::from_errno(EPIPE);
			}
			if (is_nonblock && !inode->can_write())
				return BAN::Error::from_errno(EAGAIN);
			nwrite = TRY(inode->write(offset, buffer));
		}

		LockGuard _(m_mutex);
		// NOTE: race condition with offset, its UB per POSIX
		if (!validate_fd(fd).is_error())
			m_open_files[fd].offset() = offset + nwrite;
		return nwrite;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read_dir_entries(int fd, struct dirent* list, size_t list_len)
	{
		BAN::RefPtr<Inode> inode;
		off_t offset;

		{
			LockGuard _(m_mutex);
			TRY(validate_fd(fd));
			auto& open_file = m_open_files[fd];
			if (!(open_file.status_flags() & O_RDONLY))
				return BAN::Error::from_errno(EACCES);
			inode = open_file.inode();
			offset = open_file.offset();
		}

		for (;;)
		{
			auto ret = inode->list_next_inodes(offset, list, list_len);
			if (ret.is_error() && ret.error().get_error_code() != ENODATA)
				return ret;
			offset++;
			if (ret.is_error())
				continue;

			LockGuard _(m_mutex);
			// NOTE: race condition with offset, its UB per POSIX
			if (!validate_fd(fd).is_error())
				m_open_files[fd].offset() = offset;
			return ret;
		}
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::recvmsg(int fd, msghdr& message, int flags)
	{
		BAN::RefPtr<Inode> inode;
		bool is_nonblock;

		{
			LockGuard _(m_mutex);
			TRY(validate_fd(fd));
			auto& open_file = m_open_files[fd];
			if (!open_file.inode()->mode().ifsock())
				return BAN::Error::from_errno(ENOTSOCK);
			inode = open_file.inode();
			is_nonblock = !!(open_file.status_flags() & O_NONBLOCK);
		}

		LockGuard _(inode->m_mutex);
		if (is_nonblock && !inode->can_read())
			return BAN::Error::from_errno(EWOULDBLOCK);
		return inode->recvmsg(message, flags);
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::sendmsg(int fd, const msghdr& message, int flags)
	{
		BAN::RefPtr<Inode> inode;
		bool is_nonblock;

		{
			LockGuard _(m_mutex);
			TRY(validate_fd(fd));
			auto& open_file = m_open_files[fd];
			if (!open_file.inode()->mode().ifsock())
				return BAN::Error::from_errno(ENOTSOCK);
			inode = open_file.inode();
			is_nonblock = !!(open_file.status_flags() & O_NONBLOCK);
		}

		LockGuard _(inode->m_mutex);
		if (inode->has_hungup())
		{
			Thread::current().add_signal(SIGPIPE, {});
			return BAN::Error::from_errno(EPIPE);
		}
		if (is_nonblock && !inode->can_write())
			return BAN::Error::from_errno(EWOULDBLOCK);
		return inode->sendmsg(message, flags);
	}

	BAN::ErrorOr<VirtualFileSystem::File> OpenFileDescriptorSet::file_of(int fd) const
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return TRY(m_open_files[fd].description->file.clone());
	}

	BAN::ErrorOr<BAN::String> OpenFileDescriptorSet::path_of(int fd) const
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		BAN::String path;
		TRY(path.append(m_open_files[fd].path()));
		return path;
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> OpenFileDescriptorSet::inode_of(int fd)
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return m_open_files[fd].inode();
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::status_flags_of(int fd) const
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return m_open_files[fd].status_flags();
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::validate_fd(int fd) const
	{
		LockGuard _(m_mutex);
		if (fd < 0 || fd >= (int)m_open_files.size())
			return BAN::Error::from_errno(EBADF);
		if (!m_open_files[fd].description)
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::get_free_fd() const
	{
		LockGuard _(m_mutex);
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
			if (!m_open_files[fd].description)
				return fd;
		return BAN::Error::from_errno(EMFILE);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::get_free_fd_pair(int fds[2]) const
	{
		LockGuard _(m_mutex);
		size_t found = 0;
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (!m_open_files[fd].description)
				fds[found++] = fd;
			if (found == 2)
				return {};
		}
		return BAN::Error::from_errno(EMFILE);
	}

	using FDWrapper = OpenFileDescriptorSet::FDWrapper;

	FDWrapper::FDWrapper(BAN::RefPtr<OpenFileDescription> description)
		: m_description(description)
	{
		if (m_description)
			m_description->file.inode->on_clone(m_description->status_flags);
	}

	FDWrapper::~FDWrapper()
	{
		clear();
	}

	FDWrapper& FDWrapper::operator=(const FDWrapper& other)
	{
		clear();
		m_description = other.m_description;
		if (m_description)
			m_description->file.inode->on_clone(m_description->status_flags);
		return *this;
	}

	FDWrapper& FDWrapper::operator=(FDWrapper&& other)
	{
		clear();
		m_description = BAN::move(other.m_description);
		return *this;
	}

	void FDWrapper::clear()
	{
		if (m_description)
			m_description->file.inode->on_close(m_description->status_flags);
	}

	BAN::ErrorOr<FDWrapper> OpenFileDescriptorSet::get_fd_wrapper(int fd)
	{
		LockGuard _(m_mutex);
		TRY(validate_fd(fd));
		return FDWrapper { m_open_files[fd].description };
	}

	size_t OpenFileDescriptorSet::open_all_fd_wrappers(BAN::Span<FDWrapper> fd_wrappers)
	{
		LockGuard _(m_mutex);

		for (size_t i = 0; i < fd_wrappers.size(); i++)
		{
			auto fd_or_error = get_free_fd();
			if (fd_or_error.is_error())
				return i;

			const int fd = fd_or_error.release_value();
			m_open_files[fd].description = BAN::move(fd_wrappers[i].m_description);
			m_open_files[fd].descriptor_flags = 0;
			fd_wrappers[i].m_fd = fd;
		}

		return fd_wrappers.size();
	}

}
