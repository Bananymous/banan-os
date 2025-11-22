#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Timer/Timer.h>

#include <sys/stat.h>

namespace Kernel
{

	TmpInodeInfo create_inode_info(mode_t mode, uid_t uid, gid_t gid)
	{
		auto current_time = SystemTimer::get().real_time();

		TmpInodeInfo info;
		info.uid = uid;
		info.gid = gid;
		info.mode = mode;
		info.atime = current_time;
		info.mtime = current_time;
		info.ctime = current_time;

		return info;
	}

	static uint8_t inode_mode_to_dt_type(Inode::Mode mode)
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
		ASSERT_NOT_REACHED();
	}

	/* GENERAL INODE */

	const FileSystem* TmpInode::filesystem() const
	{
		return &m_fs;
	}

	dev_t TmpInode::dev() const
	{
		return m_fs.rdev();
	}

	BAN::ErrorOr<BAN::RefPtr<TmpInode>> TmpInode::create_from_existing(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
	{
		TmpInode* inode_ptr = nullptr;
		switch (info.mode & Mode::TYPE_MASK)
		{
			case Mode::IFDIR:
				inode_ptr = new TmpDirectoryInode(fs, ino, info);
				break;
			case Mode::IFREG:
				inode_ptr = new TmpFileInode(fs, ino, info);
				break;
			default:
				ASSERT_NOT_REACHED();
		}
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<TmpInode>::adopt(inode_ptr);
	}

	TmpInode::TmpInode(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
		: m_fs(fs)
		, m_inode_info(info)
		, m_ino(ino)
	{
		// FIXME: this should be able to fail
		MUST(fs.add_to_cache(this));
	}

	TmpInode::~TmpInode()
	{
		if (nlink() > 0)
		{
			sync();
			return;
		}
		free_all_blocks();
		m_fs.delete_inode(ino());
	}

	BAN::ErrorOr<void> TmpInode::chmod_impl(mode_t new_mode)
	{
		ASSERT(!(new_mode & Inode::Mode::TYPE_MASK));
		m_inode_info.mode &= Inode::Mode::TYPE_MASK;
		m_inode_info.mode |= new_mode;
		return {};
	}

	BAN::ErrorOr<void> TmpInode::chown_impl(uid_t new_uid, gid_t new_gid)
	{
		m_inode_info.uid = new_uid;
		m_inode_info.gid = new_gid;
		return {};
	}

	BAN::ErrorOr<void> TmpInode::utimens_impl(const timespec times[2])
	{
		if (times[0].tv_nsec != UTIME_OMIT)
			m_inode_info.atime = times[0];
		if (times[1].tv_nsec != UTIME_OMIT)
			m_inode_info.atime = times[1];
		return {};
	}

	void TmpInode::sync()
	{
		m_fs.write_inode(m_ino, m_inode_info);
	}

	void TmpInode::free_all_blocks()
	{
		for (size_t i = 0; i < TmpInodeInfo::direct_block_count; i++)
			if (m_inode_info.block[i])
				m_fs.free_block(m_inode_info.block[i]);
		if (size_t block = m_inode_info.block[TmpInodeInfo::direct_block_count + 0])
			free_indirect_blocks(block, 1);
		if (size_t block = m_inode_info.block[TmpInodeInfo::direct_block_count + 1])
			free_indirect_blocks(block, 2);
		if (size_t block = m_inode_info.block[TmpInodeInfo::direct_block_count + 2])
			free_indirect_blocks(block, 3);
		for (auto& block : m_inode_info.block)
			block = 0;
	}

	void TmpInode::free_indirect_blocks(size_t block, uint32_t depth)
	{
		ASSERT(block != 0);

		if (depth == 0)
		{
			m_fs.free_block(block);
			return;
		}

		const size_t indices_per_block = blksize() / sizeof(size_t);
		for (size_t index = 0; index < indices_per_block; index++)
		{
			size_t next_block;
			m_fs.with_block_buffer(block, [&](BAN::ByteSpan block_buffer) {
				next_block = block_buffer.as_span<size_t>()[index];
			});

			if (next_block == 0)
				continue;

			free_indirect_blocks(next_block, depth - 1);
		}

		m_fs.free_block(block);
	}

	BAN::Optional<size_t> TmpInode::block_index(size_t data_block_index)
	{
		if (data_block_index < TmpInodeInfo::direct_block_count)
		{
			if (m_inode_info.block[data_block_index] == 0)
				return {};
			return m_inode_info.block[data_block_index];
		}
		data_block_index -= TmpInodeInfo::direct_block_count;

		const size_t indices_per_block = blksize() / sizeof(size_t);

		if (data_block_index < indices_per_block)
			return block_index_from_indirect(m_inode_info.block[TmpInodeInfo::direct_block_count + 0], data_block_index, 1);
		data_block_index -= indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block)
			return block_index_from_indirect(m_inode_info.block[TmpInodeInfo::direct_block_count + 1], data_block_index, 2);
		data_block_index -= indices_per_block * indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block * indices_per_block)
			return block_index_from_indirect(m_inode_info.block[TmpInodeInfo::direct_block_count + 2], data_block_index, 3);

		ASSERT_NOT_REACHED();
	}

	BAN::Optional<size_t> TmpInode::block_index_from_indirect(size_t block, size_t index, uint32_t depth)
	{
		if (block == 0)
			return {};
		ASSERT(depth >= 1);

		const size_t indices_per_block = blksize() / sizeof(size_t);

		size_t divisor = 1;
		for (size_t i = 1; i < depth; i++)
			divisor *= indices_per_block;

		size_t next_block;
		m_fs.with_block_buffer(block, [&](BAN::ByteSpan block_buffer) {
			next_block = block_buffer.as_span<size_t>()[(index / divisor) % indices_per_block];
		});

		if (next_block == 0)
			return {};

		if (depth == 1)
			return next_block;

		return block_index_from_indirect(next_block, index, depth - 1);
	}

	BAN::ErrorOr<size_t> TmpInode::block_index_with_allocation(size_t data_block_index)
	{
		if (data_block_index < TmpInodeInfo::direct_block_count)
		{
			if (m_inode_info.block[data_block_index] == 0)
			{
				m_inode_info.block[data_block_index] = TRY(m_fs.allocate_block());
				m_inode_info.blocks++;
			}
			return m_inode_info.block[data_block_index];
		}
		data_block_index -= TmpInodeInfo::direct_block_count;

		const size_t indices_per_block = blksize() / sizeof(size_t);

		if (data_block_index < indices_per_block)
			return block_index_from_indirect_with_allocation(m_inode_info.block[TmpInodeInfo::direct_block_count + 0], data_block_index, 1);
		data_block_index -= indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block)
			return block_index_from_indirect_with_allocation(m_inode_info.block[TmpInodeInfo::direct_block_count + 1], data_block_index, 2);
		data_block_index -= indices_per_block * indices_per_block;

		if (data_block_index < indices_per_block * indices_per_block * indices_per_block)
			return block_index_from_indirect_with_allocation(m_inode_info.block[TmpInodeInfo::direct_block_count + 2], data_block_index, 3);

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<size_t> TmpInode::block_index_from_indirect_with_allocation(size_t& block, size_t index, uint32_t depth)
	{
		if (block == 0)
		{
			block = TRY(m_fs.allocate_block());
			m_inode_info.blocks++;
		}
		ASSERT(depth >= 1);

		const size_t indices_per_block = blksize() / sizeof(size_t);

		size_t divisor = 1;
		for (size_t i = 1; i < depth; i++)
			divisor *= indices_per_block;

		size_t next_block;
		m_fs.with_block_buffer(block, [&](BAN::ByteSpan block_buffer) {
			next_block = block_buffer.as_span<size_t>()[(index / divisor) % indices_per_block];
		});

		if (next_block == 0)
		{
			next_block = TRY(m_fs.allocate_block());
			m_inode_info.blocks++;

			m_fs.with_block_buffer(block, [&](BAN::ByteSpan block_buffer) {
				block_buffer.as_span<size_t>()[(index / divisor) % indices_per_block] = next_block;
			});
		}

		if (depth == 1)
			return next_block;

		return block_index_from_indirect_with_allocation(next_block, index, depth - 1);
	}

	/* FILE INODE */

	BAN::ErrorOr<BAN::RefPtr<TmpFileInode>> TmpFileInode::create_new(TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto info = create_inode_info(Mode::IFREG | mode, uid, gid);
		ino_t ino = TRY(fs.allocate_inode(info));

		auto* inode_ptr = new TmpFileInode(fs, ino, info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		return BAN::RefPtr<TmpFileInode>::adopt(inode_ptr);
	}

	TmpFileInode::TmpFileInode(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
		: TmpInode(fs, ino, info)
	{
		ASSERT(mode().ifreg());
	}

	TmpFileInode::~TmpFileInode()
	{
	}

	BAN::ErrorOr<size_t> TmpFileInode::read_impl(off_t offset, BAN::ByteSpan out_buffer)
	{
		if (offset >= size() || out_buffer.size() == 0)
			return 0;

		const size_t bytes_to_read = BAN::Math::min<size_t>(size() - offset, out_buffer.size());

		size_t read_done = 0;
		while (read_done < bytes_to_read)
		{
			const size_t data_block_index = (read_done + offset) / blksize();
			const size_t block_offset     = (read_done + offset) % blksize();

			const auto block_index = this->block_index(data_block_index);

			const size_t bytes = BAN::Math::min<size_t>(bytes_to_read - read_done, blksize() - block_offset);

			if (block_index.has_value())
				m_fs.with_block_buffer(block_index.value(), [&](BAN::ByteSpan block_buffer) {
					memcpy(out_buffer.data() + read_done, block_buffer.data() + block_offset, bytes);
				});
			else
				memset(out_buffer.data() + read_done, 0x00, bytes);

			read_done += bytes;
		}

		return read_done;
	}

	BAN::ErrorOr<size_t> TmpFileInode::write_impl(off_t offset, BAN::ConstByteSpan in_buffer)
	{
		// FIXME: handle overflow

		if (offset + in_buffer.size() > (size_t)size())
			TRY(truncate_impl(offset + in_buffer.size()));

		const size_t bytes_to_write = in_buffer.size();

		size_t write_done = 0;
		while (write_done < bytes_to_write)
		{
			const size_t data_block_index = (write_done + offset) / blksize();
			const size_t block_offset     = (write_done + offset) % blksize();

			const size_t block_index = TRY(block_index_with_allocation(data_block_index));

			const size_t bytes = BAN::Math::min<size_t>(bytes_to_write - write_done, blksize() - block_offset);

			m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan block_buffer) {
				memcpy(block_buffer.data() + block_offset, in_buffer.data() + write_done, bytes);
			});

			write_done += bytes;
		}

		return write_done;
	}

	BAN::ErrorOr<void> TmpFileInode::truncate_impl(size_t new_size)
	{
		// FIXME: if size is decreased, we should probably free
		//        unused blocks

		m_inode_info.size = new_size;
		return {};
	}

	/* SOCKET INODE */
	BAN::ErrorOr<BAN::RefPtr<TmpSocketInode>> TmpSocketInode::create_new(TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto info = create_inode_info(Mode::IFSOCK | mode, uid, gid);
		ino_t ino = TRY(fs.allocate_inode(info));

		auto* inode_ptr = new TmpSocketInode(fs, ino, info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		return BAN::RefPtr<TmpSocketInode>::adopt(inode_ptr);
	}

	TmpSocketInode::TmpSocketInode(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
		: TmpInode(fs, ino, info)
	{
		ASSERT(mode().ifsock());
	}

	TmpSocketInode::~TmpSocketInode()
	{
	}

	/* SYMLINK INODE */

	BAN::ErrorOr<BAN::RefPtr<TmpSymlinkInode>> TmpSymlinkInode::create_new(TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid, BAN::StringView target)
	{
		auto info = create_inode_info(Mode::IFLNK | mode, uid, gid);
		ino_t ino = TRY(fs.allocate_inode(info));

		auto* inode_ptr = new TmpSymlinkInode(fs, ino, info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto inode = BAN::RefPtr<TmpSymlinkInode>::adopt(inode_ptr);

		TRY(inode->set_link_target(target));

		return inode;
	}

	TmpSymlinkInode::TmpSymlinkInode(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
		: TmpInode(fs, ino, info)
	{
		ASSERT(mode().iflnk());
	}

	TmpSymlinkInode::~TmpSymlinkInode()
	{
	}

	BAN::ErrorOr<void> TmpSymlinkInode::set_link_target_impl(BAN::StringView new_target)
	{
		free_all_blocks();
		m_inode_info.size = 0;

		if (new_target.size() <= sizeof(TmpInodeInfo::block))
		{
			memcpy(m_inode_info.block.data(), new_target.data(), new_target.size());
			m_inode_info.size = new_target.size();
			return {};
		}

		const size_t blocks_needed = BAN::Math::div_round_up<size_t>(new_target.size(), blksize());
		for (size_t i = 0; i < blocks_needed; i++)
		{
			const size_t block_index = TRY(block_index_with_allocation(i));
			const size_t byte_count = BAN::Math::min<size_t>(new_target.size() - i * blksize(), blksize());

			m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan bytespan) {
				memcpy(bytespan.data(), new_target.data() + i * blksize(), byte_count);
			});

			m_inode_info.size += byte_count;
		}

		return {};
	}

	BAN::ErrorOr<BAN::String> TmpSymlinkInode::link_target_impl()
	{
		BAN::String result;
		TRY(result.resize(size()));

		if ((size_t)size() <= sizeof(TmpInodeInfo::block))
		{
			memcpy(result.data(), m_inode_info.block.data(), size());
			return result;
		}

		const size_t data_block_count = BAN::Math::div_round_up<size_t>(size(), blksize());
		for (size_t i = 0; i < data_block_count; i++)
		{
			const size_t block_index = TRY(block_index_with_allocation(i));
			const size_t byte_count = BAN::Math::min<size_t>(size() - i * blksize(), blksize());

			m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan bytespan) {
				memcpy(result.data() + i * blksize(), bytespan.data(), byte_count);
			});
		}

		return result;
	}

	/* DIRECTORY INODE */

	BAN::ErrorOr<BAN::RefPtr<TmpDirectoryInode>> TmpDirectoryInode::create_root(TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto info = create_inode_info(Mode::IFDIR | mode, uid, gid);
		ino_t ino = TRY(fs.allocate_inode(info));

		auto* inode_ptr = new TmpDirectoryInode(fs, ino, info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		auto inode = BAN::RefPtr<TmpDirectoryInode>::adopt(inode_ptr);
		TRY(inode->link_inode(*inode, "."_sv));
		TRY(inode->link_inode(*inode, ".."_sv));

		return inode;
	}

	BAN::ErrorOr<BAN::RefPtr<TmpDirectoryInode>> TmpDirectoryInode::create_new(TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid, TmpInode& parent)
	{
		auto info = create_inode_info(Mode::IFDIR | mode, uid, gid);
		ino_t ino = TRY(fs.allocate_inode(info));

		auto* inode_ptr = new TmpDirectoryInode(fs, ino, info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		auto inode = BAN::RefPtr<TmpDirectoryInode>::adopt(inode_ptr);
		TRY(inode->link_inode(*inode, "."_sv));
		TRY(inode->link_inode(parent, ".."_sv));

		return inode;
	}

	TmpDirectoryInode::TmpDirectoryInode(TmpFileSystem& fs, ino_t ino, const TmpInodeInfo& info)
		: TmpInode(fs, ino, info)
	{
		ASSERT(mode().ifdir());
	}

	TmpDirectoryInode::~TmpDirectoryInode()
	{
	}

	BAN::ErrorOr<void> TmpDirectoryInode::prepare_unlink()
	{
		ino_t dot_ino = 0;
		ino_t dotdot_ino = 0;

		bool is_empty = true;
		for_each_valid_entry([&](TmpDirectoryEntry& entry) {
			if (entry.name_sv() == "."_sv)
				dot_ino = entry.ino;
			else if (entry.name_sv() == ".."_sv)
				dotdot_ino = entry.ino;
			else
			{
				is_empty = false;
				return BAN::Iteration::Break;
			}
			return BAN::Iteration::Continue;
		});
		if (!is_empty)
			return BAN::Error::from_errno(ENOTEMPTY);

		// FIXME: can these leak inodes?

		if (dot_ino)
		{
			auto inode = TRY(m_fs.open_inode(dot_ino));
			ASSERT(inode->nlink() > 0);
			inode->m_inode_info.nlink--;
		}

		if (dotdot_ino)
		{
			auto inode = TRY(m_fs.open_inode(dotdot_ino));
			ASSERT(inode->nlink() > 0);
			inode->m_inode_info.nlink--;
		}

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> TmpDirectoryInode::find_inode_impl(BAN::StringView name)
	{
		ino_t result = 0;

		for_each_valid_entry([&](TmpDirectoryEntry& entry) {
			if (entry.name_sv() != name)
				return BAN::Iteration::Continue;
			result = entry.ino;
			return BAN::Iteration::Break;
		});

		if (result == 0)
			return BAN::Error::from_errno(ENOENT);

		auto inode = TRY(m_fs.open_inode(result));
		return BAN::RefPtr<Inode>(inode);
	}

	BAN::ErrorOr<size_t> TmpDirectoryInode::list_next_inodes_impl(off_t data_block_index, struct dirent* list, size_t list_len)
	{
		if (list_len == 0)
		{
			dprintln("buffer is too small");
			return BAN::Error::from_errno(ENOBUFS);
		}

		auto block_index = this->block_index(data_block_index);

		// if we reach a non-allocated block, it marks the end
		if (!block_index.has_value())
			return 0;

		dirent* dirp = list;
		size_t entry_count = 0;

		const size_t byte_count = BAN::Math::min<size_t>(size() - data_block_index * blksize(), blksize());
		m_fs.with_block_buffer(block_index.value(), [&](BAN::ByteSpan bytespan) {
			bytespan = bytespan.slice(0, byte_count);

			while (bytespan.size() > 0)
			{
				const auto& entry = bytespan.as<TmpDirectoryEntry>();

				if (entry.type != DT_UNKNOWN)
				{
					// TODO: dirents should be aligned

					entry_count++;
					if (entry_count > list_len)
						return;

					dirp->d_ino = entry.ino;
					dirp->d_type = entry.type;
					strncpy(dirp->d_name, entry.name, entry.name_len);
					dirp->d_name[entry.name_len] = '\0';
					dirp++;
				}

				bytespan = bytespan.slice(entry.rec_len);
			}
		});

		if (entry_count == 0)
			return BAN::Error::from_errno(ENODATA);
		return entry_count;
	}

	BAN::ErrorOr<void> TmpDirectoryInode::create_file_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		BAN::RefPtr<TmpInode> new_inode;
		switch (mode & Mode::TYPE_MASK)
		{
			case Mode::IFREG:
				new_inode = TRY(TmpFileInode::create_new(m_fs, mode, uid, gid));
				break;
			case Mode::IFLNK:
				new_inode = TRY(TmpSymlinkInode::create_new(m_fs, mode, uid, gid, ""_sv));
				break;
			case Mode::IFSOCK:
				new_inode = TRY(TmpSocketInode::create_new(m_fs, mode, uid, gid));
				break;
			default:
				dprintln("Creating with mode {o} is not supported", mode);
				return BAN::Error::from_errno(ENOTSUP);
		}
		TRY(link_inode(*new_inode, name));
		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::create_directory_impl(BAN::StringView name, mode_t mode, uid_t uid, gid_t gid)
	{
		auto new_inode = TRY(TmpDirectoryInode::create_new(m_fs, mode, uid, gid, *this));
		TRY(link_inode(*new_inode, name));
		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::link_inode_impl(BAN::StringView name, BAN::RefPtr<Inode> inode)
	{
		ASSERT(this->mode().ifdir());
		ASSERT(!inode->mode().ifdir());
		ASSERT(&m_fs == inode->filesystem());

		if (!find_inode_impl(name).is_error())
			return BAN::Error::from_errno(EEXIST);

		auto* tmp_inode = static_cast<TmpInode*>(inode.ptr());
		TRY(link_inode(*tmp_inode, name));
		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::rename_inode_impl(BAN::RefPtr<Inode> old_parent, BAN::StringView old_name, BAN::StringView new_name)
	{
		ASSERT(this->mode().ifdir());
		ASSERT(old_parent->mode().ifdir());
		ASSERT(&m_fs == old_parent->filesystem());

		auto* tmp_parent = static_cast<TmpDirectoryInode*>(old_parent.ptr());

		// FIXME: possible deadlock :)
		LockGuard _(tmp_parent->m_mutex);

		auto old_inode = TRY(tmp_parent->find_inode_impl(old_name));
		auto* tmp_inode = static_cast<TmpInode*>(old_inode.ptr());

		if (auto replace_or_error = find_inode_impl(new_name); replace_or_error.is_error())
		{
			if (replace_or_error.error().get_error_code() != ENOENT)
				return replace_or_error.release_error();
		}
		else
		{
			TRY(unlink_impl(new_name));
		}

		TRY(link_inode(*tmp_inode, new_name));

		TRY(tmp_parent->unlink_inode(old_name, false));

		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::unlink_impl(BAN::StringView name)
	{
		TRY(unlink_inode(name, true));
		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::unlink_inode(BAN::StringView name, bool cleanup)
	{
		ino_t entry_ino = 0;

		for_each_valid_entry([&](TmpDirectoryEntry& entry) {
			if (entry.name_sv() != name)
				return BAN::Iteration::Continue;
			entry_ino = entry.ino;
			return BAN::Iteration::Break;
		});

		if (entry_ino == 0)
			return BAN::Error::from_errno(ENOENT);

		auto inode = TRY(m_fs.open_inode(entry_ino));

		ASSERT(inode->nlink() > 0);

		if (cleanup)
			TRY(inode->prepare_unlink());
		inode->m_inode_info.nlink--;

		if (inode->nlink() == 0)
			m_fs.remove_from_cache(inode);

		for_each_valid_entry([&](TmpDirectoryEntry& entry) {
			if (entry.name_sv() != name)
				return BAN::Iteration::Continue;
			entry.ino = 0;
			entry.type = DT_UNKNOWN;
			return BAN::Iteration::Break;
		});

		return {};
	}

	BAN::ErrorOr<void> TmpDirectoryInode::link_inode(TmpInode& inode, BAN::StringView name)
	{
		static constexpr size_t directory_entry_alignment = sizeof(TmpDirectoryEntry);

		auto find_result = find_inode_impl(name);
		if (!find_result.is_error())
			return BAN::Error::from_errno(EEXIST);
		if (find_result.error().get_error_code() != ENOENT)
			return find_result.release_error();

		size_t new_entry_size = sizeof(TmpDirectoryEntry) + name.size();
		if (auto rem = new_entry_size % directory_entry_alignment)
			new_entry_size += directory_entry_alignment - rem;
		ASSERT(new_entry_size < (size_t)blksize());

		for (size_t data_block_index = 0; data_block_index * blksize() < (size_t)size(); data_block_index++)
		{
			const size_t block_index = this->block_index(data_block_index).value();

			bool done = false;
			m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan bytespan) {
				bytespan = bytespan.slice(0, blksize());
				while (bytespan.size() > 0)
				{
					auto& entry = bytespan.as<TmpDirectoryEntry>();

					while (entry.ino == 0 && entry.rec_len < bytespan.size())
					{
						auto& next_entry = bytespan.slice(entry.rec_len).as<TmpDirectoryEntry>();
						if (next_entry.ino)
							break;
						entry.rec_len += next_entry.rec_len;
					}

					if (entry.ino != 0 || entry.rec_len < new_entry_size)
					{
						bytespan = bytespan.slice(entry.rec_len);
						continue;
					}

					if (entry.rec_len <= new_entry_size + sizeof(TmpDirectoryEntry))
						new_entry_size = entry.rec_len;
					else
					{
						auto& new_entry = bytespan.slice(new_entry_size).as<TmpDirectoryEntry>();
						new_entry.type = DT_UNKNOWN;
						new_entry.ino = 0;
						new_entry.rec_len = entry.rec_len - new_entry_size;
					}

					ASSERT(entry.type == DT_UNKNOWN);
					entry.type = inode_mode_to_dt_type(inode.mode());
					entry.ino = inode.ino();
					entry.name_len = name.size();
					entry.rec_len = new_entry_size;
					memcpy(entry.name, name.data(), name.size());

					done = true;
					break;
				}
			});

			if (done)
			{
				// add link to linked inode
				inode.m_inode_info.nlink++;
				return {};
			}
		}

		const size_t data_block_index = size() / blksize();
		const size_t block_index = TRY(block_index_with_allocation(data_block_index));

		m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan bytespan) {
			// insert new inode
			{
				auto& entry = bytespan.as<TmpDirectoryEntry>();
				entry.type = inode_mode_to_dt_type(inode.mode());
				entry.ino = inode.ino();
				entry.name_len = name.size();
				entry.rec_len = new_entry_size;
				memcpy(entry.name, name.data(), name.size());
			}

			// insert null entry
			{
				auto& entry = bytespan.slice(new_entry_size).as<TmpDirectoryEntry>();
				entry.type = DT_UNKNOWN;
				entry.ino = 0;
				entry.rec_len = blksize() - new_entry_size;
			}
		});

		// increase current size
		m_inode_info.size += blksize();

		// add link to linked inode
		inode.m_inode_info.nlink++;

		return {};
	}

	template<TmpFuncs::for_each_valid_entry_callback F>
	void TmpDirectoryInode::for_each_valid_entry(F callback)
	{
		bool done = false;
		for (size_t data_block_index = 0; !done && data_block_index * blksize() < (size_t)size(); data_block_index++)
		{
			const size_t block_index = this->block_index(data_block_index).value();
			const size_t byte_count = BAN::Math::min<size_t>(size() - data_block_index * blksize(), blksize());

			m_fs.with_block_buffer(block_index, [&](BAN::ByteSpan bytespan) {
				bytespan = bytespan.slice(0, byte_count);
				while (bytespan.size() > 0)
				{
					auto& entry = bytespan.as<TmpDirectoryEntry>();
					if (entry.ino != 0)
					{
						switch (callback(entry))
						{
							case BAN::Iteration::Continue:
								break;
							case BAN::Iteration::Break:
								done = true;
								return;
							default:
								ASSERT_NOT_REACHED();
						}
					}
					bytespan = bytespan.slice(entry.rec_len);
				}
			});
		}
	}

}
