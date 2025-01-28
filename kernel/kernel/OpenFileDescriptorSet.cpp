#include <kernel/FS/Pipe.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/OpenFileDescriptorSet.h>

#include <fcntl.h>
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
		for (size_t i = 0; i < m_open_files.size(); i++)
			m_open_files[i] = BAN::move(other.m_open_files[i]);
		return *this;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::clone_from(const OpenFileDescriptorSet& other)
	{
		close_all();

		for (int fd = 0; fd < (int)other.m_open_files.size(); fd++)
		{
			if (other.validate_fd(fd).is_error())
				continue;

			auto& open_file = m_open_files[fd];
			open_file.description      = other.m_open_files[fd].description;
			open_file.descriptor_flags = other.m_open_files[fd].descriptor_flags;

			if (open_file.path() == "<pipe wr>"_sv)
			{
				ASSERT(open_file.inode()->is_pipe());
				static_cast<Pipe*>(open_file.inode().ptr())->clone_writing();
			}
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

		constexpr int status_mask = O_APPEND | O_DSYNC | O_NONBLOCK | O_RSYNC | O_SYNC | O_ACCMODE;
		int fd = TRY(get_free_fd());
		m_open_files[fd].description = TRY(BAN::RefPtr<OpenFileDescription>::create(BAN::move(file), 0, flags & status_mask));
		m_open_files[fd].descriptor_flags = flags & O_CLOEXEC;

		return fd;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::open(BAN::StringView absolute_path, int flags)
	{
		return open(TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, flags)), flags);
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::socket(int domain, int type, int protocol)
	{
		bool valid_protocol = true;

		Socket::Domain sock_domain;
		switch (domain)
		{
			case AF_INET:
				sock_domain = Socket::Domain::INET;
				break;
			case AF_INET6:
				sock_domain = Socket::Domain::INET6;
				break;
			case AF_UNIX:
				sock_domain = Socket::Domain::UNIX;
				valid_protocol = false;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		int status_flags = 0;
		int descriptor_flags = 0;
		if (type & SOCK_NONBLOCK)
			status_flags |= O_NONBLOCK;
		if (type & SOCK_CLOEXEC)
			descriptor_flags |= O_CLOEXEC;
		type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

		Socket::Type sock_type;
		switch (type)
		{
			case SOCK_STREAM:
				sock_type = Socket::Type::STREAM;
				if (protocol != IPPROTO_TCP)
					valid_protocol = false;
				break;
			case SOCK_DGRAM:
				sock_type = Socket::Type::DGRAM;
				if (protocol != IPPROTO_UDP)
					valid_protocol = false;
				break;
			case SOCK_SEQPACKET:
				sock_type = Socket::Type::SEQPACKET;
				valid_protocol = false;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		if (protocol && !valid_protocol)
			return BAN::Error::from_errno(EPROTONOSUPPORT);

		auto socket = TRY(NetworkManager::get().create_socket(sock_domain, sock_type, 0777, m_credentials.euid(), m_credentials.egid()));

		int fd = TRY(get_free_fd());
		m_open_files[fd].description = TRY(BAN::RefPtr<OpenFileDescription>::create(VirtualFileSystem::File(socket, "<socket>"_sv), 0, O_RDWR | status_flags));
		m_open_files[fd].descriptor_flags = descriptor_flags;
		return fd;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::pipe(int fds[2])
	{
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

		TRY(validate_fd(fildes));
		if (fildes == fildes2)
			return fildes;

		(void)close(fildes2);

		m_open_files[fildes2].description = m_open_files[fildes].description;
		m_open_files[fildes2].descriptor_flags = 0;

		if (m_open_files[fildes2].path() == "<pipe wr>"_sv)
		{
			ASSERT(m_open_files[fildes2].inode()->is_pipe());
			static_cast<Pipe*>(m_open_files[fildes2].inode().ptr())->clone_writing();
		}

		return fildes;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::fcntl(int fd, int cmd, int extra)
	{
		TRY(validate_fd(fd));

		switch (cmd)
		{
			case F_DUPFD:
			case F_DUPFD_CLOEXEC:
			{
				const int new_fd = TRY(get_free_fd());

				m_open_files[new_fd].description = m_open_files[fd].description;
				m_open_files[new_fd].descriptor_flags = (cmd == F_DUPFD_CLOEXEC) ? O_CLOEXEC : 0;
				if (m_open_files[new_fd].path() == "<pipe wr>"_sv)
				{
					ASSERT(m_open_files[new_fd].inode()->is_pipe());
					static_cast<Pipe*>(m_open_files[new_fd].inode().ptr())->clone_writing();
				}

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
				m_open_files[fd].status_flags() &= ~O_ACCMODE;
				m_open_files[fd].status_flags() |= extra;
				return 0;
			default:
				break;
		}

		dwarnln("TODO: fcntl command {}", cmd);
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<off_t> OpenFileDescriptorSet::seek(int fd, off_t offset, int whence)
	{
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
		TRY(validate_fd(fd));
		return m_open_files[fd].offset();
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::truncate(int fd, off_t length)
	{
		TRY(validate_fd(fd));
		return m_open_files[fd].inode()->truncate(length);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::close(int fd)
	{
		TRY(validate_fd(fd));

		if (m_open_files[fd].path() == "<pipe wr>"_sv)
		{
			ASSERT(m_open_files[fd].inode()->is_pipe());
			static_cast<Pipe*>(m_open_files[fd].inode().ptr())->close_writing();
		}

		m_open_files[fd].description.clear();
		m_open_files[fd].descriptor_flags = 0;

		return {};
	}

	void OpenFileDescriptorSet::close_all()
	{
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
			(void)close(fd);
	}

	void OpenFileDescriptorSet::close_cloexec()
	{
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (validate_fd(fd).is_error())
				continue;
			if (m_open_files[fd].descriptor_flags & O_CLOEXEC)
				(void)close(fd);
		}
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read(int fd, BAN::ByteSpan buffer)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if (!(open_file.status_flags() & O_RDONLY))
			return BAN::Error::from_errno(EBADF);
		if ((open_file.status_flags() & O_NONBLOCK) && !open_file.inode()->can_read())
			return 0;
		size_t nread = TRY(open_file.inode()->read(open_file.offset(), buffer));
		open_file.offset() += nread;
		return nread;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::write(int fd, BAN::ConstByteSpan buffer)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if (!(open_file.status_flags() & O_WRONLY))
			return BAN::Error::from_errno(EBADF);
		if ((open_file.status_flags() & O_NONBLOCK) && !open_file.inode()->can_write())
			return 0;
		if (open_file.status_flags() & O_APPEND)
			open_file.offset() = open_file.inode()->size();
		size_t nwrite = TRY(open_file.inode()->write(open_file.offset(), buffer));
		open_file.offset() += nwrite;
		return nwrite;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read_dir_entries(int fd, struct dirent* list, size_t list_len)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if (!(open_file.status_flags() & O_RDONLY))
			return BAN::Error::from_errno(EACCES);
		for (;;)
		{
			auto ret = open_file.inode()->list_next_inodes(open_file.offset(), list, list_len);
			if (ret.is_error() && ret.error().get_error_code() != ENODATA)
				return ret;
			open_file.offset()++;
			if (ret.is_error())
				continue;
			return ret;
		}
	}

	BAN::ErrorOr<VirtualFileSystem::File> OpenFileDescriptorSet::file_of(int fd) const
	{
		TRY(validate_fd(fd));
		return TRY(m_open_files[fd].description->file.clone());
	}

	BAN::ErrorOr<BAN::StringView> OpenFileDescriptorSet::path_of(int fd) const
	{
		TRY(validate_fd(fd));
		return m_open_files[fd].path();
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> OpenFileDescriptorSet::inode_of(int fd)
	{
		TRY(validate_fd(fd));
		return m_open_files[fd].inode();
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::status_flags_of(int fd) const
	{
		TRY(validate_fd(fd));
		return m_open_files[fd].status_flags();
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::validate_fd(int fd) const
	{
		if (fd < 0 || fd >= (int)m_open_files.size())
			return BAN::Error::from_errno(EBADF);
		if (!m_open_files[fd].description)
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::get_free_fd() const
	{
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
			if (!m_open_files[fd].description)
				return fd;
		return BAN::Error::from_errno(EMFILE);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::get_free_fd_pair(int fds[2]) const
	{
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

}
