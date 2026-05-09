#include <kernel/Epoll.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Inode.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>

#include <fcntl.h>
#include <sys/statvfs.h>

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
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return find_inode_impl(name);
	}

	BAN::ErrorOr<size_t> Inode::list_next_inodes(off_t offset, struct dirent* list, size_t list_len)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return list_next_inodes_impl(offset, list, list_len);
	}

	BAN::ErrorOr<void> Inode::create_file(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return create_file_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::create_directory(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (!Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return create_directory_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::link_inode(BAN::StringView name, BAN::RefPtr<Inode> inode)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (inode->mode().ifdir())
			return BAN::Error::from_errno(EINVAL);
		if (this->filesystem() != inode->filesystem())
			return BAN::Error::from_errno(EXDEV);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return link_inode_impl(name, inode);
	}

	BAN::ErrorOr<void> Inode::rename_inode(BAN::RefPtr<Inode> old_parent, BAN::StringView old_name, BAN::StringView new_name)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (!old_parent->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (this->filesystem() != old_parent->filesystem())
			return BAN::Error::from_errno(EXDEV);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return rename_inode_impl(old_parent, old_name, new_name);
	}

	BAN::ErrorOr<void> Inode::unlink(BAN::StringView name)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (name == "."_sv || name == ".."_sv)
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return unlink_impl(name);
	}

	BAN::ErrorOr<BAN::String> Inode::link_target()
	{
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		return link_target_impl();
	}

	BAN::ErrorOr<void> Inode::set_link_target(BAN::StringView target)
	{
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return set_link_target_impl(target);
	}

	BAN::ErrorOr<long> Inode::accept(sockaddr* address, socklen_t* address_len, int flags)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return accept_impl(address, address_len, flags);
	}

	BAN::ErrorOr<void> Inode::bind(const sockaddr* address, socklen_t address_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return bind_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::connect(const sockaddr* address, socklen_t address_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return connect_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::listen(int backlog)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return listen_impl(backlog);
	}

	BAN::ErrorOr<size_t> Inode::recvmsg(msghdr& message, int flags)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return recvmsg_impl(message, flags);
	}

	BAN::ErrorOr<size_t> Inode::sendmsg(const msghdr& message, int flags)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return sendmsg_impl(message, flags);
	}

	BAN::ErrorOr<void> Inode::getsockname(sockaddr* address, socklen_t* address_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return getsockname_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::getpeername(sockaddr* address, socklen_t* address_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return getpeername_impl(address, address_len);
	}

	BAN::ErrorOr<void> Inode::getsockopt(int level, int option, void* value, socklen_t* value_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return getsockopt_impl(level, option, value, value_len);
	}

	BAN::ErrorOr<void> Inode::setsockopt(int level, int option, const void* value, socklen_t value_len)
	{
		if (!mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);
		return setsockopt_impl(level, option, value, value_len);
	}

	BAN::ErrorOr<size_t> Inode::read(off_t offset, BAN::ByteSpan buffer)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return read_impl(offset, buffer);
	}

	BAN::ErrorOr<size_t> Inode::write(off_t offset, BAN::ConstByteSpan buffer)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return write_impl(offset, buffer);
	}

	BAN::ErrorOr<void> Inode::truncate(size_t size)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return truncate_impl(size);
	}

	BAN::ErrorOr<void> Inode::chmod(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return chmod_impl(mode);
	}

	BAN::ErrorOr<void> Inode::chown(uid_t uid, gid_t gid)
	{
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return chown_impl(uid, gid);
	}

	BAN::ErrorOr<void> Inode::utimens(const timespec times[2])
	{
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		return utimens_impl(times);
	}

	BAN::ErrorOr<void> Inode::fsync()
	{
		// TODO: should we sync shared data?
		return fsync_impl();
	}

	bool Inode::can_read() const
	{
		return can_read_impl();
	}

	bool Inode::can_write() const
	{
		return can_write_impl();
	}

	bool Inode::has_error() const
	{
		return has_error_impl();
	}

	bool Inode::has_hungup() const
	{
		return has_hungup_impl();
	}

	BAN::ErrorOr<long> Inode::ioctl(int request, void* arg)
	{
		return ioctl_impl(request, arg);
	}

	BAN::ErrorOr<void> Inode::add_epoll(class Epoll* epoll)
	{
		SpinLockGuard _(m_epoll_lock);
		TRY(m_epolls.push_back(epoll));
		return {};
	}

	void Inode::del_epoll(class Epoll* epoll)
	{
		SpinLockGuard _(m_epoll_lock);
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
		SpinLockGuard _(m_epoll_lock);
		for (auto* epoll : m_epolls)
			epoll->notify(this, event);
	}

}
