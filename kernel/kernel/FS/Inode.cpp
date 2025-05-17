#include <kernel/Epoll.h>
#include <kernel/FS/Inode.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>

#include <fcntl.h>

namespace Kernel
{

	bool Inode::can_access(const Credentials& credentials, int flags) const
	{
		if (credentials.is_superuser())
			return true;

		if (flags & O_RDONLY)
		{
			if (mode().mode & S_IROTH)
			{ }
			else if ((mode().mode & S_IRUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IRGRP) && credentials.has_egid(gid()))
			{ }
			else
			{
				return false;
			}
		}

		if (flags & O_WRONLY)
		{
			if (mode().mode & S_IWOTH)
			{ }
			else if ((mode().mode & S_IWUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IWGRP) && credentials.has_egid(gid()))
			{ }
			else
			{
				return false;
			}
		}

		if ((flags & O_EXEC) || (mode().ifdir() && (flags & O_SEARCH)))
		{
			if (mode().mode & S_IXOTH)
			{ }
			else if ((mode().mode & S_IXUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IXGRP) && credentials.has_egid(gid()))
			{ }
			else
			{
				return false;
			}
		}

		return true;
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Inode::find_inode(BAN::StringView name)
	{
		LockGuard _(m_mutex);
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return find_inode_impl(name);
	}

	BAN::ErrorOr<size_t> Inode::list_next_inodes(off_t offset, struct dirent* list, size_t list_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return list_next_inodes_impl(offset, list, list_len);
	}

	BAN::ErrorOr<void> Inode::create_file(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		LockGuard _(m_mutex);
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		return create_file_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::create_directory(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		LockGuard _(m_mutex);
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (!Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		return create_directory_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::link_inode(BAN::StringView name, BAN::RefPtr<Inode> inode)
	{
		LockGuard _(m_mutex);
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (inode->mode().ifdir())
			return BAN::Error::from_errno(EINVAL);
		return link_inode_impl(name, inode);
	}

	BAN::ErrorOr<void> Inode::unlink(BAN::StringView name)
	{
		LockGuard _(m_mutex);
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (name == "."_sv || name == ".."_sv)
			return BAN::Error::from_errno(EINVAL);
		return unlink_impl(name);
	}

	BAN::ErrorOr<BAN::String> Inode::link_target()
	{
		LockGuard _(m_mutex);
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		return link_target_impl();
	}

	BAN::ErrorOr<void> Inode::set_link_target(BAN::StringView target)
	{
		LockGuard _(m_mutex);
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		return set_link_target_impl(target);
	}

	BAN::ErrorOr<long> Inode::accept(sockaddr* address, socklen_t* address_len, int flags)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return accept_impl(address, address_len, flags);
	}

	BAN::ErrorOr<void> Inode::bind(const sockaddr* address, socklen_t address_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return bind_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::connect(const sockaddr* address, socklen_t address_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return connect_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::listen(int backlog)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return listen_impl(backlog);
	}

	BAN::ErrorOr<size_t> Inode::sendto(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return sendto_impl(message, address, address_len);
	};

	BAN::ErrorOr<size_t> Inode::recvfrom(BAN::ByteSpan buffer, sockaddr* address, socklen_t* address_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return recvfrom_impl(buffer, address, address_len);
	};

	BAN::ErrorOr<void> Inode::getsockname(sockaddr* address, socklen_t* address_len)
	{
		LockGuard _(m_mutex);
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return getsockname_impl(address, address_len);
	}

	BAN::ErrorOr<size_t> Inode::read(off_t offset, BAN::ByteSpan buffer)
	{
		LockGuard _(m_mutex);
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return read_impl(offset, buffer);
	}

	BAN::ErrorOr<size_t> Inode::write(off_t offset, BAN::ConstByteSpan buffer)
	{
		LockGuard _(m_mutex);
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return write_impl(offset, buffer);
	}

	BAN::ErrorOr<void> Inode::truncate(size_t size)
	{
		LockGuard _(m_mutex);
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return truncate_impl(size);
	}

	BAN::ErrorOr<void> Inode::chmod(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		LockGuard _(m_mutex);
		return chmod_impl(mode);
	}

	BAN::ErrorOr<void> Inode::chown(uid_t uid, gid_t gid)
	{
		LockGuard _(m_mutex);
		return chown_impl(uid, gid);
	}

	BAN::ErrorOr<void> Inode::fsync()
	{
		LockGuard _(m_mutex);
		if (auto shared = m_shared_region.lock())
			for (size_t i = 0; i < shared->pages.size(); i++)
				shared->sync(i);
		return fsync_impl();
	}

	bool Inode::can_read() const
	{
		LockGuard _(m_mutex);
		return can_read_impl();
	}

	bool Inode::can_write() const
	{
		LockGuard _(m_mutex);
		return can_write_impl();
	}

	bool Inode::has_error() const
	{
		LockGuard _(m_mutex);
		return has_error_impl();
	}

	bool Inode::has_hangup() const
	{
		LockGuard _(m_mutex);
		return has_hangup_impl();
	}

	BAN::ErrorOr<long> Inode::ioctl(int request, void* arg)
	{
		LockGuard _(m_mutex);
		return ioctl_impl(request, arg);
	}

	BAN::ErrorOr<void> Inode::add_epoll(class Epoll* epoll)
	{
		LockGuard _(m_epoll_mutex);
		TRY(m_epolls.push_back(epoll));
		return {};
	}

	void Inode::del_epoll(class Epoll* epoll)
	{
		LockGuard _(m_epoll_mutex);
		for (auto it = m_epolls.begin(); it != m_epolls.end(); it++)
		{
			if (*it != epoll)
				continue;
			m_epolls.remove(it);
			break;
		}
	}

	void Inode::epoll_notify(uint32_t event)
	{
		LockGuard _(m_epoll_mutex);
		for (auto* epoll : m_epolls)
			epoll->notify(this, event);
	}

}
