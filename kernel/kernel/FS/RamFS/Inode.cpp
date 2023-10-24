#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	RamInode::FullInodeInfo::FullInodeInfo(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		timespec current_time = SystemTimer::get().real_time();

		this->ino = fs.next_ino();
		this->mode = mode;
		this->nlink = 1;
		this->uid = uid;
		this->gid = gid;
		this->size = 0;
		this->atime = current_time;
		this->mtime = current_time;
		this->ctime = current_time;
		this->blksize = fs.blksize();
		this->blocks = 0;

		// TODO
		this->dev = 0;
		this->rdev = 0;
	}


	BAN::ErrorOr<void> RamInode::chmod_impl(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		m_inode_info.mode = (m_inode_info.mode & Inode::Mode::TYPE_MASK) | mode;
		return {};
	}

	/*

		RAM FILE INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamFileInode>> RamFileInode::create(RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		FullInodeInfo inode_info(fs, mode, uid, gid);

		auto* ram_inode = new RamFileInode(fs, inode_info);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamFileInode>::adopt(ram_inode);
	}

	RamFileInode::RamFileInode(RamFileSystem& fs, const FullInodeInfo& inode_info)
		: RamInode(fs, inode_info)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> RamFileInode::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		ASSERT(offset >= 0);
		if (offset >= size())
			return 0;
		size_t to_copy = BAN::Math::min<size_t>(m_inode_info.size - offset, buffer.size());
		memcpy(buffer.data(), m_data.data(), to_copy);
		return to_copy;
	}

	BAN::ErrorOr<size_t> RamFileInode::write_impl(off_t offset, BAN::ConstByteSpan buffer)
	{
		ASSERT(offset >= 0);
		if (offset + buffer.size() > (size_t)size())
			TRY(truncate_impl(offset + buffer.size()));
		memcpy(m_data.data() + offset, buffer.data(), buffer.size());
		return buffer.size();
	}

	BAN::ErrorOr<void> RamFileInode::truncate_impl(size_t new_size)
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
		FullInodeInfo inode_info(fs, mode, uid, gid);

		// "." links to this
		inode_info.nlink++;

		// ".." links to this or parent
		if (parent)
			TRY(fs.get_inode(parent))->add_link();
		else
		{
			inode_info.nlink++;
			parent = inode_info.ino;
		}

		auto* ram_inode = new RamDirectoryInode(fs, inode_info, parent);
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamDirectoryInode>::adopt(ram_inode);
	}

	RamDirectoryInode::RamDirectoryInode(RamFileSystem& fs, const FullInodeInfo& inode_info, ino_t parent)
		: RamInode(fs, inode_info)
		, m_parent(parent)
	{
		m_inode_info.mode |= Inode::Mode::IFDIR;
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
			ptr->dirent.d_type = DT_DIR;
			ptr->rec_len = sizeof(DirectoryEntry) + 2;
			strcpy(ptr->dirent.d_name, ".");
			ptr = ptr->next();
		}

		// ".."
		{
			ptr->dirent.d_ino = m_parent;
			ptr->dirent.d_type = DT_DIR;
			ptr->rec_len = sizeof(DirectoryEntry) + 3;
			strcpy(ptr->dirent.d_name, "..");
			ptr = ptr->next();
		}

		for (auto& entry : m_entries)
		{
			ptr->dirent.d_ino = entry.ino;
			ptr->dirent.d_type = entry.type;
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
			inode = TRY(RamFileInode::create(m_fs, mode & ~Inode::Mode::TYPE_MASK, uid, gid));
		else if (Mode(mode).ifdir())
			inode = TRY(RamDirectoryInode::create(m_fs, ino(), mode & ~Inode::Mode::TYPE_MASK, uid, gid));
		else
			ASSERT_NOT_REACHED();

		TRY(add_inode(name, inode));

		return {};
	}

	static uint8_t get_type(Inode::Mode mode)
	{
		if (mode.ifreg())
			return DT_REG;
		if (mode.ifdir())
			return DT_DIR;
		if (mode.ifchr())
			return DT_CHR;
		if (mode.ifblk())
			return DT_BLK;
		if (mode.ififo())
			return DT_FIFO;
		if (mode.ifsock())
			return DT_SOCK;
		if (mode.iflnk())
			return DT_LNK;
		return DT_UNKNOWN;
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
		entry.type = get_type(inode->mode());

		if (auto ret = m_fs.add_inode(inode); ret.is_error())
		{
			m_entries.pop_back();
			return ret.release_error();
		}

		return {};
	}

	BAN::ErrorOr<void> RamDirectoryInode::delete_inode_impl(BAN::StringView name)
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (name == m_entries[i].name)
			{
				m_entries.remove(i);
				return {};
			}
		}
		return BAN::Error::from_errno(ENOENT);
	}

	/*

		RAM SYMLINK INODE

	*/

	BAN::ErrorOr<BAN::RefPtr<RamSymlinkInode>> RamSymlinkInode::create(RamFileSystem& fs, BAN::StringView target_sv, mode_t mode, uid_t uid, gid_t gid)
	{
		FullInodeInfo inode_info(fs, mode, uid, gid);

		BAN::String target_str;
		TRY(target_str.append(target_sv));

		auto* ram_inode = new RamSymlinkInode(fs, inode_info, BAN::move(target_str));
		if (ram_inode == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RamSymlinkInode>::adopt(ram_inode);
	}

	RamSymlinkInode::RamSymlinkInode(RamFileSystem& fs, const FullInodeInfo& inode_info, BAN::String&& target)
		: RamInode(fs, inode_info)
		, m_target(BAN::move(target))
	{
		m_inode_info.mode |= Inode::Mode::IFLNK;
	}

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