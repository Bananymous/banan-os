#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	/*

		RAM INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamInode>> RamInode::create(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(Mode(mode).ifreg());
		auto* ram_inode = new RamInode(fs, mode, uid, gid);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamInode>::adopt(ram_inode);
	}

	RamInode::RamInode(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
			: m_fs(fs)
	{
		timespec current_time = SystemTimer::get().real_time();

		m_inode_info.ino = fs.next_ino();
		m_inode_info.mode = mode;
		m_inode_info.nlink = 1;
		m_inode_info.uid = uid;
		m_inode_info.gid = gid;
		m_inode_info.size = 0;
		m_inode_info.atime = current_time;
		m_inode_info.mtime = current_time;
		m_inode_info.ctime = current_time;
		m_inode_info.blksize = fs.blksize();
		m_inode_info.blocks = 0;
		m_inode_info.dev = 0;
		m_inode_info.rdev = 0;
	}

	BAN::ErrorOr<size_t> RamInode::read_impl(off_t offset, void* buffer, size_t bytes)
	{
		ASSERT(offset >= 0);
		if (offset >= size())
			return 0;
		size_t to_copy = BAN::Math::min<size_t>(m_inode_info.size - offset, bytes);
		memcpy(buffer, m_data.data(), to_copy);
		return to_copy;
	}

	BAN::ErrorOr<size_t> RamInode::write_impl(off_t offset, const void* buffer, size_t bytes)
	{
		ASSERT(offset >= 0);
		if (offset + bytes > (size_t)size())
			TRY(truncate_impl(offset + bytes));
		memcpy(m_data.data() + offset, buffer, bytes);
		return bytes;
	}

	BAN::ErrorOr<void> RamInode::truncate_impl(size_t new_size)
	{
		TRY(m_data.resize(new_size, 0));
		m_inode_info.size   = m_data.size();
		m_inode_info.blocks = BAN::Math::div_round_up<size_t>(size(), blksize());
		return {};
	}

	/*

		RAM DIRECTORY INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamDirectoryInode>> RamDirectoryInode::create(RamFileSystem& fs, ino_t parent, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(Mode(mode).ifdir());
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

	BAN::ErrorOr<BAN::RefPtr<Inode>> RamDirectoryInode::find_inode_impl(BAN::StringView name)
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

		return BAN::Error::from_errno(ENOENT);
	}

	BAN::ErrorOr<void> RamDirectoryInode::list_next_inodes_impl(off_t offset, DirectoryEntryList* list, size_t list_size)
	{
		ASSERT(offset >= 0);

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

	BAN::ErrorOr<void> RamDirectoryInode::create_file_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		BAN::RefPtr<RamInode> inode;
		if (Mode(mode).ifreg())
			inode = TRY(RamInode::create(m_fs, mode, uid, gid));
		else if (Mode(mode).ifdir())
			inode = TRY(RamDirectoryInode::create(m_fs, ino(), mode, uid, gid));
		else
			ASSERT_NOT_REACHED();

		TRY(add_inode(name, inode));

		return {};
	}

	BAN::ErrorOr<void> RamDirectoryInode::add_inode(BAN::StringView name, BAN::RefPtr<RamInode> inode)
	{
		if (name.size() > m_name_max)
			return BAN::Error::from_errno(ENAMETOOLONG);
		
		for (auto& entry : m_entries)
			if (name == entry.name)
				return BAN::Error::from_errno(EEXIST);
		
		TRY(m_entries.push_back({ }));
		Entry& entry = m_entries.back();
		strcpy(entry.name, name.data());
		entry.name_len = name.size();
		entry.ino = inode->ino();

		if (auto ret = m_fs.add_inode(inode); ret.is_error())
		{
			m_entries.pop_back();
			return ret.release_error();
		}

		return {};
	}

	/*

		RAM SYMLINK INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamSymlinkInode>> RamSymlinkInode::create(RamFileSystem& fs, BAN::StringView target, mode_t mode, uid_t uid, gid_t gid)
	{
		ASSERT(Mode(mode).iflnk());
		auto* ram_inode = new RamSymlinkInode(fs, mode, uid, gid);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto ref_ptr = BAN::RefPtr<RamSymlinkInode>::adopt(ram_inode);
		TRY(ref_ptr->set_link_target(target));
		return ref_ptr;
	}

	RamSymlinkInode::RamSymlinkInode(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
		: RamInode(fs, mode, uid, gid)
	{ }

	BAN::ErrorOr<BAN::String> RamSymlinkInode::link_target_impl()
	{
		BAN::String result;
		TRY(result.append(m_target));
		return result;
	}

	BAN::ErrorOr<void> RamSymlinkInode::set_link_target(BAN::StringView target)
	{
		BAN::String temp;
		TRY(temp.append(target));
		m_target = BAN::move(temp);
		return {};
	}

}