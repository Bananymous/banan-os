#include <kernel/FS/Inode.h>
#include <kernel/LockGuard.h>
#include <kernel/Thread.h>

#include <fcntl.h>

namespace Kernel
{

	bool Inode::can_access(const Credentials& credentials, int flags)
	{
		if (credentials.is_superuser())
			return true;

		if (flags & O_RDONLY)
		{
			if (mode().mode & S_IROTH)
			{ }
			else if ((mode().mode & S_IRUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IRGRP) && credentials.egid() == gid())
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
			else if ((mode().mode & S_IWGRP) && credentials.egid() == gid())
			{ }
			else
			{
				return false;
			}
		}

		if (flags & (O_EXEC | O_SEARCH))
		{
			if (mode().mode & S_IXOTH)
			{ }
			else if ((mode().mode & S_IXUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IXGRP) && credentials.egid() == gid())
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
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return find_inode_impl(name);
	}

	BAN::ErrorOr<void> Inode::list_next_inodes(off_t offset, DirectoryEntryList* list, size_t list_len)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		return list_next_inodes_impl(offset, list, list_len);
	}

	BAN::ErrorOr<void> Inode::create_file(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		return create_file_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::create_directory(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (!Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		return create_directory_impl(name, mode, uid, gid);
	}

	BAN::ErrorOr<void> Inode::unlink(BAN::StringView name)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (name == "."sv || name == ".."sv)
			return BAN::Error::from_errno(EINVAL);
		return unlink_impl(name);
	}

	BAN::ErrorOr<BAN::String> Inode::link_target()
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		return link_target_impl();
	}

	BAN::ErrorOr<size_t> Inode::read(off_t offset, BAN::ByteSpan buffer)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return read_impl(offset, buffer);
	}

	BAN::ErrorOr<size_t> Inode::write(off_t offset, BAN::ConstByteSpan buffer)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return write_impl(offset, buffer);
	}

	BAN::ErrorOr<void> Inode::truncate(size_t size)
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		return truncate_impl(size);
	}

	BAN::ErrorOr<void> Inode::chmod(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		return chmod_impl(mode);
	}

	bool Inode::has_data() const
	{
		LockGuard _(m_lock);
		Thread::TerminateBlocker blocker(Thread::current());
		return has_data_impl();
	}

}