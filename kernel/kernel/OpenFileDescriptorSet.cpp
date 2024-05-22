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

			auto& open_file = other.m_open_files[fd];

			auto result = BAN::RefPtr<OpenFileDescription>::create(open_file->inode, open_file->path, open_file->offset, open_file->flags);

			if (result.is_error())
			{
				close_all();
				return result.error();
			}

			m_open_files[fd] = result.release_value();

			if (m_open_files[fd]->flags & O_WRONLY && m_open_files[fd]->inode->is_pipe())
				((Pipe*)m_open_files[fd]->inode.ptr())->clone_writing();
		}

		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::open(BAN::RefPtr<Inode> inode, int flags)
	{
		ASSERT(inode);
		ASSERT(!inode->mode().ifdir());

		if (flags & ~(O_RDONLY | O_WRONLY))
			return BAN::Error::from_errno(ENOTSUP);

		int fd = TRY(get_free_fd());
		// FIXME: path?
		m_open_files[fd] = TRY(BAN::RefPtr<OpenFileDescription>::create(inode, ""sv, 0, flags));

		return fd;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::open(BAN::StringView absolute_path, int flags)
	{
		if (flags & ~(O_RDONLY | O_WRONLY | O_NOFOLLOW | O_SEARCH | O_APPEND | O_TRUNC | O_CLOEXEC | O_TTY_INIT | O_DIRECTORY | O_NONBLOCK))
			return BAN::Error::from_errno(ENOTSUP);

		int access_mask = O_EXEC | O_RDONLY | O_WRONLY | O_SEARCH;
		if ((flags & access_mask) != O_RDWR && __builtin_popcount(flags & access_mask) != 1)
			return BAN::Error::from_errno(EINVAL);

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, flags));

		if ((flags & O_DIRECTORY) && !file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		if ((flags & O_TRUNC) && (flags & O_WRONLY) && file.inode->mode().ifreg())
			TRY(file.inode->truncate(0));

		int fd = TRY(get_free_fd());
		m_open_files[fd] = TRY(BAN::RefPtr<OpenFileDescription>::create(file.inode, BAN::move(file.canonical_path), 0, flags));

		return fd;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::socket(int domain, int type, int protocol)
	{
		if (protocol != 0)
			return BAN::Error::from_errno(EPROTONOSUPPORT);

		SocketDomain sock_domain;
		switch (domain)
		{
			case AF_INET:
				sock_domain = SocketDomain::INET;
				break;
			case AF_INET6:
				sock_domain = SocketDomain::INET6;
				break;
			case AF_UNIX:
				sock_domain = SocketDomain::UNIX;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		SocketType sock_type;
		switch (type)
		{
			case SOCK_STREAM:
				sock_type = SocketType::STREAM;
				break;
			case SOCK_DGRAM:
				sock_type = SocketType::DGRAM;
				break;
			case SOCK_SEQPACKET:
				sock_type = SocketType::SEQPACKET;
				break;
			default:
				return BAN::Error::from_errno(EPROTOTYPE);
		}

		auto socket = TRY(NetworkManager::get().create_socket(sock_domain, sock_type, 0777, m_credentials.euid(), m_credentials.egid()));

		int fd = TRY(get_free_fd());
		m_open_files[fd] = TRY(BAN::RefPtr<OpenFileDescription>::create(socket, "no-path"sv, 0, O_RDWR));
		return fd;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::pipe(int fds[2])
	{
		TRY(get_free_fd_pair(fds));

		auto pipe = TRY(Pipe::create(m_credentials));
		m_open_files[fds[0]] = TRY(BAN::RefPtr<OpenFileDescription>::create(pipe, ""sv, 0, O_RDONLY));
		m_open_files[fds[1]] = TRY(BAN::RefPtr<OpenFileDescription>::create(pipe, ""sv, 0, O_WRONLY));

		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::dup(int fildes)
	{
		TRY(validate_fd(fildes));

		int result = TRY(get_free_fd());
		m_open_files[result] = m_open_files[fildes];

		if (m_open_files[result]->flags & O_WRONLY && m_open_files[result]->inode->is_pipe())
			((Pipe*)m_open_files[result]->inode.ptr())->clone_writing();

		return result;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::dup2(int fildes, int fildes2)
	{
		if (fildes2 < 0 || fildes2 >= (int)m_open_files.size())
			return BAN::Error::from_errno(EBADF);

		TRY(validate_fd(fildes));
		if (fildes == fildes2)
			return fildes;

		(void)close(fildes2);

		m_open_files[fildes2] = m_open_files[fildes];
		m_open_files[fildes2]->flags &= ~O_CLOEXEC;

		if (m_open_files[fildes]->flags & O_WRONLY && m_open_files[fildes]->inode->is_pipe())
			((Pipe*)m_open_files[fildes]->inode.ptr())->clone_writing();

		return fildes;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::fcntl(int fd, int cmd, int extra)
	{
		TRY(validate_fd(fd));

		constexpr int creation_flags = O_CLOEXEC | O_CREAT | O_DIRECTORY | O_EXCL | O_NOCTTY | O_NOFOLLOW | O_TRUNC | O_TTY_INIT;

		switch (cmd)
		{
			case F_GETFD:
				return m_open_files[fd]->flags;
			case F_SETFD:
				// FIXME: validate permissions to set access flags
				m_open_files[fd]->flags = extra;
				return 0;
			case F_GETFL:
				return m_open_files[fd]->flags & ~creation_flags;
			case F_SETFL:
				m_open_files[fd]->flags |= extra & ~(O_ACCMODE | creation_flags);
				return 0;
			default:
				break;
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::seek(int fd, off_t offset, int whence)
	{
		TRY(validate_fd(fd));

		off_t new_offset = 0;

		switch (whence)
		{
			case SEEK_CUR:
				new_offset = m_open_files[fd]->offset + offset;
				break;
			case SEEK_END:
				new_offset = m_open_files[fd]->inode->size() - offset;
				break;
			case SEEK_SET:
				new_offset = offset;
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		if (new_offset < 0)
			return BAN::Error::from_errno(EINVAL);

		m_open_files[fd]->offset = new_offset;

		return {};
	}

	BAN::ErrorOr<off_t> OpenFileDescriptorSet::tell(int fd) const
	{
		TRY(validate_fd(fd));
		return m_open_files[fd]->offset;
	}

	static void read_stat_from_inode(BAN::RefPtr<Inode> inode, struct stat* out)
	{
		out->st_dev		= inode->dev();
		out->st_ino		= inode->ino();
		out->st_mode	= inode->mode().mode;
		out->st_nlink	= inode->nlink();
		out->st_uid		= inode->uid();
		out->st_gid		= inode->gid();
		out->st_rdev	= inode->rdev();
		out->st_size	= inode->size();
		out->st_atim	= inode->atime();
		out->st_mtim	= inode->mtime();
		out->st_ctim	= inode->ctime();
		out->st_blksize	= inode->blksize();
		out->st_blocks	= inode->blocks();
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::fstat(int fd, struct stat* out) const
	{
		TRY(validate_fd(fd));
		read_stat_from_inode(m_open_files[fd]->inode, out);
		return {};
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::fstatat(int fd, BAN::StringView path, struct stat* out, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		BAN::String absolute_path;
		TRY(absolute_path.append(TRY(path_of(fd))));
		TRY(absolute_path.push_back('/'));
		TRY(absolute_path.append(path));

		// FIXME: handle O_SEARCH in fd
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, flag));
		read_stat_from_inode(file.inode, out);

		return {};
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::stat(BAN::StringView absolute_path, struct stat* out, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, flag));
		read_stat_from_inode(file.inode, out);
		return {};
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::close(int fd)
	{
		TRY(validate_fd(fd));

		if (m_open_files[fd]->flags & O_WRONLY && m_open_files[fd]->inode->is_pipe())
			((Pipe*)m_open_files[fd]->inode.ptr())->close_writing();

		m_open_files[fd]->inode->on_close();

		m_open_files[fd].clear();

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
			if (m_open_files[fd]->flags & O_CLOEXEC)
				(void)close(fd);
		}
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read(int fd, BAN::ByteSpan buffer)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if ((open_file->flags & O_NONBLOCK) && !open_file->inode->can_read())
			return 0;
		size_t nread = TRY(open_file->inode->read(open_file->offset, buffer));
		open_file->offset += nread;
		return nread;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::write(int fd, BAN::ConstByteSpan buffer)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if ((open_file->flags & O_NONBLOCK) && !open_file->inode->can_write())
			return 0;
		if (open_file->flags & O_APPEND)
			open_file->offset = open_file->inode->size();
		size_t nwrite = TRY(open_file->inode->write(open_file->offset, buffer));
		open_file->offset += nwrite;
		return nwrite;
	}

	BAN::ErrorOr<size_t> OpenFileDescriptorSet::read_dir_entries(int fd, struct dirent* list, size_t list_len)
	{
		TRY(validate_fd(fd));
		auto& open_file = m_open_files[fd];
		if (!(open_file->flags & O_RDONLY))
			return BAN::Error::from_errno(EACCES);
		return TRY(open_file->inode->list_next_inodes(open_file->offset++, list, list_len));
	}

	BAN::ErrorOr<BAN::StringView> OpenFileDescriptorSet::path_of(int fd) const
	{
		TRY(validate_fd(fd));
		return m_open_files[fd]->path.sv();
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> OpenFileDescriptorSet::inode_of(int fd)
	{
		TRY(validate_fd(fd));
		return m_open_files[fd]->inode;
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::flags_of(int fd) const
	{
		TRY(validate_fd(fd));
		return m_open_files[fd]->flags;
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::validate_fd(int fd) const
	{
		if (fd < 0 || fd >= (int)m_open_files.size())
			return BAN::Error::from_errno(EBADF);
		if (!m_open_files[fd])
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	BAN::ErrorOr<int> OpenFileDescriptorSet::get_free_fd() const
	{
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
			if (!m_open_files[fd])
				return fd;
		return BAN::Error::from_errno(EMFILE);
	}

	BAN::ErrorOr<void> OpenFileDescriptorSet::get_free_fd_pair(int fds[2]) const
	{
		size_t found = 0;
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (!m_open_files[fd])
				fds[found++] = fd;
			if (found == 2)
				return {};
		}
		return BAN::Error::from_errno(EMFILE);
	}

}
