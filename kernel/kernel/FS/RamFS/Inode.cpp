#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/RTC.h>

namespace Kernel
{

	/*
	
		RAM INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamInode>> RamInode::create(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(Mode{ mode }.ifreg());
		auto* ram_inode = new RamInode(fs, mode, uid, gid);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamInode>::adopt(ram_inode);
	}

	RamInode::RamInode(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
			: m_fs(fs)
	{
		uint64_t current_unix_time = BAN::to_unix_time(RTC::get_current_time());
		timespec current_timespec;
		current_timespec.tv_sec = current_unix_time;
		current_timespec.tv_nsec = 0;

		m_inode_info.ino = fs.next_ino();
		m_inode_info.mode = mode;
		m_inode_info.nlink = 1;
		m_inode_info.uid = uid;
		m_inode_info.gid = gid;
		m_inode_info.size = 0;
		m_inode_info.atime = current_timespec;
		m_inode_info.mtime = current_timespec;
		m_inode_info.ctime = current_timespec;
		m_inode_info.blksize = fs.blksize();
		m_inode_info.blocks = 0;
		m_inode_info.dev = 0;
		m_inode_info.rdev = 0;
	}

	/*
	
		RAM DIRECTORY INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamDirectoryInode>> RamDirectoryInode::create(RamFileSystem& fs, ino_t parent, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(Mode{ mode }.ifdir());
		auto* ram_inode = new RamDirectoryInode(fs, parent, mode, uid, gid);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamDirectoryInode>::adopt(ram_inode);
	}

	RamDirectoryInode::RamDirectoryInode(RamFileSystem& fs, ino_t parent, mode_t mode, uid_t uid, gid_t gid)
		: RamInode(fs, mode, uid, gid)
	{
		// "." links to this
		m_inode_info.nlink++;

		// ".." links to this, if there is no parent
		if (parent == 0)
		{
			m_inode_info.nlink++;
			m_parent = ino();
		}
		else
		{
			MUST(fs.get_inode(parent))->add_link();
			m_parent = parent;
		}
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> RamDirectoryInode::directory_find_inode(BAN::StringView name)
	{
		if (name == "."sv)
		{
			BAN::RefPtr<Inode> inode = TRY(m_fs.get_inode(ino()));
			return inode;
		}

		if (name == ".."sv)
		{
			BAN::RefPtr<Inode> inode = TRY(m_fs.get_inode(m_parent));
			return inode;
		}

		for (const auto& entry : m_entries)
		{
			if (name == entry.name)
			{
				BAN::RefPtr<Inode> inode = TRY(m_fs.get_inode(entry.ino));
				return inode;
			}
		}

		sizeof(Entry);

		return BAN::Error::from_errno(ENOENT);
	}

	BAN::ErrorOr<void> RamDirectoryInode::directory_read_next_entries(off_t offset, DirectoryEntryList* list, size_t list_size)
	{
		// TODO: don't require memory for all entries on single call
		if (offset != 0)
		{
			list->entry_count = 0;
			return {};
		}

		size_t needed_size = sizeof(DirectoryEntryList);
		needed_size += sizeof(DirectoryEntry) + 2; // "."
		needed_size += sizeof(DirectoryEntry) + 3; // ".."
		for (auto& entry : m_entries)
			needed_size += sizeof(DirectoryEntry) + entry.name_len + 1;
		if (needed_size > list_size)
			return BAN::Error::from_errno(EINVAL);

		DirectoryEntry* ptr = list->array;

		// "."
		{
			ptr->dirent.d_ino = ino();
			ptr->rec_len = sizeof(DirectoryEntry) + 2;
			strcpy(ptr->dirent.d_name, ".");
			ptr = ptr->next();
		}

		// ".."
		{
			ptr->dirent.d_ino = m_parent;
			ptr->rec_len = sizeof(DirectoryEntry) + 3;
			strcpy(ptr->dirent.d_name, "..");
			ptr = ptr->next();
		}

		for (auto& entry : m_entries)
		{
			ptr->dirent.d_ino = entry.ino;
			ptr->rec_len = sizeof(DirectoryEntry) + entry.name_len + 1;
			strcpy(ptr->dirent.d_name, entry.name);
			ptr = ptr->next();
		}

		list->entry_count = m_entries.size() + 2;

		return {};
	}

	BAN::ErrorOr<void> RamDirectoryInode::create_file(BAN::StringView, mode_t)
	{
		ASSERT_NOT_REACHED();
	}

}