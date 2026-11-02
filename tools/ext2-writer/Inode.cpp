#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Inode.h>
#include <kernel/Timer/Timer.h>

#include <sys/statvfs.h>

namespace Kernel
{


	void Inode::update_timestamp(uint8_t update)
	{
		const auto timestamp = SystemTimer::get().real_time();
		if (update & ATIME) m_atime = timestamp;
		if (update & MTIME) m_mtime = timestamp;
		if (update & CTIME) m_ctime = timestamp;
		if (auto ret = sync_inode(SyncType::Times); ret.is_error())
			dwarnln("sync timestamp: {}", ret.error());
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
		const auto ret = TRY(list_next_inodes_impl(offset, list, list_len));
		update_timestamp(ATIME);
		return ret;
	}

	BAN::ErrorOr<void> Inode::create_file(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		TRY(create_file_impl(name, mode, uid, gid));
		update_timestamp(MTIME | CTIME);
		return {};
	}

	BAN::ErrorOr<void> Inode::create_directory(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		if (!this->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (!Mode(mode).ifdir())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		TRY(create_directory_impl(name, mode, uid, gid));
		update_timestamp(MTIME | CTIME);
		return {};
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
		TRY(link_inode_impl(name, inode));
		update_timestamp(MTIME | CTIME);
		return {};
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
		TRY(rename_inode_impl(old_parent, old_name, new_name));
		update_timestamp(CTIME);
		return {};
	}

	BAN::ErrorOr<void> Inode::unlink(BAN::StringView name)
	{
		if (!mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		if (name == "."_sv || name == ".."_sv)
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		TRY(unlink_impl(name));
		update_timestamp(MTIME | CTIME);
		return {};
	}

	BAN::ErrorOr<BAN::String> Inode::link_target()
	{
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		const auto ret = TRY(link_target_impl());
		update_timestamp(ATIME);
		return ret;
	}

	BAN::ErrorOr<void> Inode::set_link_target(BAN::StringView target)
	{
		if (!mode().iflnk())
			return BAN::Error::from_errno(EINVAL);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		TRY(set_link_target_impl(target));
		update_timestamp(MTIME);
		return {};
	}

	BAN::ErrorOr<size_t> Inode::read(off_t offset, BAN::ByteSpan buffer)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		const auto ret = TRY(read_impl(offset, buffer));
		update_timestamp(ATIME);
		return ret;
	}

	BAN::ErrorOr<size_t> Inode::write(off_t offset, BAN::ConstByteSpan buffer)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		const auto ret = TRY(write_impl(offset, buffer));
		update_timestamp(MTIME | CTIME);
		return ret;
	}

	BAN::ErrorOr<void> Inode::truncate(size_t size)
	{
		if (mode().ifdir())
			return BAN::Error::from_errno(EISDIR);
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);
		TRY(truncate_impl(size));
		update_timestamp(MTIME | CTIME);
		return {};
	}

	BAN::ErrorOr<void> Inode::chmod(mode_t mode)
	{
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);

		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		mode |= m_mode & Inode::Mode::TYPE_MASK;

		const auto old_mode = m_mode.exchange(mode);
		if (auto ret = sync_inode(SyncType::Mode); ret.is_error())
		{
			m_mode.compare_exchange(mode, old_mode);
			return ret.release_error();
		}

		update_timestamp(CTIME);

		return {};
	}

	BAN::ErrorOr<void> Inode::chown(uid_t uid, gid_t gid)
	{
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);

		// TODO: unify uid and gid to a single atomic operation.
		//       this needs 64 bit atomic support from 32 bit target

		const auto old_uid = m_uid.exchange(uid);
		const auto old_gid = m_gid.exchange(gid);
		if (auto ret = sync_inode(SyncType::UidGid); ret.is_error())
		{
			m_uid.compare_exchange(uid, old_uid);
			m_gid.compare_exchange(gid, old_gid);
			return ret.release_error();
		}

		update_timestamp(CTIME);

		return {};
	}

	BAN::ErrorOr<void> Inode::utimens(const timespec times[2])
	{
		if (auto* fs = filesystem(); fs && (fs->flag() & ST_RDONLY))
			return BAN::Error::from_errno(EROFS);

		// TODO: make these atomic

		const auto old_atime = m_atime;
		const auto old_mtime = m_mtime;

		m_atime = times[0];
		m_mtime = times[1];

		if (auto ret = sync_inode(SyncType::Times); ret.is_error())
		{
			m_atime = old_atime;
			m_mtime = old_mtime;
			return ret.release_error();
		}

		return {};
	}

	BAN::ErrorOr<void> Inode::fsync()
	{
		// TODO: should we sync MAP_SHARED data?
		TRY(sync_inode(SyncType::General));
		TRY(sync_data());
		return {};
	}

}
