#include <BAN/ScopeGuard.h>
#include <kernel/FS/Ext2/FileSystem.h>
#include <kernel/LockGuard.h>

#define EXT2_DEBUG_PRINT 0
#define EXT2_VERIFY_INODE 0

namespace Kernel
{

	BAN::ErrorOr<Ext2FS*> Ext2FS::create(Partition& partition)
	{
		Ext2FS* ext2fs = new Ext2FS(partition);
		if (ext2fs == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([ext2fs] { delete ext2fs; });
		TRY(ext2fs->initialize_superblock());
		TRY(ext2fs->initialize_root_inode());
		guard.disable();
		return ext2fs;
	}

	BAN::ErrorOr<void> Ext2FS::initialize_superblock()
	{
		// Read superblock from disk
		{
			const uint32_t sector_size = m_partition.device().sector_size();
			ASSERT(1024 % sector_size == 0);

			const uint32_t lba = 1024 / sector_size;
			const uint32_t sector_count = BAN::Math::div_round_up<uint32_t>(sizeof(Ext2::Superblock), sector_size);

			BAN::Vector<uint8_t> superblock_buffer;
			TRY(superblock_buffer.resize(sector_count * sector_size));

			TRY(m_partition.read_sectors(lba, sector_count, superblock_buffer.span()));

			memcpy(&m_superblock, superblock_buffer.data(), sizeof(Ext2::Superblock));
		}

		if (m_superblock.magic != Ext2::Enum::SUPER_MAGIC)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Invalid);

		if (m_superblock.rev_level == Ext2::Enum::GOOD_OLD_REV)
		{
			memset(m_superblock.__extension_start, 0, sizeof(Ext2::Superblock) - offsetof(Ext2::Superblock, Ext2::Superblock::__extension_start));
			m_superblock.first_ino = Ext2::Enum::GOOD_OLD_FIRST_INO;
			m_superblock.inode_size = Ext2::Enum::GOOD_OLD_INODE_SIZE;
		}

		const uint32_t number_of_block_groups       = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		const uint32_t number_of_block_groups_check = BAN::Math::div_round_up(superblock().blocks_count, superblock().blocks_per_group);
		if (number_of_block_groups != number_of_block_groups_check)
			return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);

		if (!(m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_FILETYPE))
		{
			dwarnln("Directory entries without filetype not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_COMPRESSION)
		{
			dwarnln("Required FEATURE_INCOMPAT_COMPRESSION");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_JOURNAL_DEV)
		{
			dwarnln("Required FEATURE_INCOMPAT_JOURNAL_DEV");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_META_BG)
		{
			dwarnln("Required FEATURE_INCOMPAT_META_BG");
			return BAN::Error::from_errno(ENOTSUP);
		}
		if (m_superblock.feature_incompat & Ext2::Enum::FEATURE_INCOMPAT_RECOVER)
		{
			dwarnln("Required FEATURE_INCOMPAT_RECOVER");
			return BAN::Error::from_errno(ENOTSUP);
		}

#if EXT2_DEBUG_PRINT
		dprintln("EXT2");
		dprintln("  inodes        {}", m_superblock.inodes_count);
		dprintln("  blocks        {}", m_superblock.blocks_count);
		dprintln("  version       {}.{}", m_superblock.rev_level, m_superblock.minor_rev_level);
		dprintln("  first data at {}", m_superblock.first_data_block);
		dprintln("  block size    {}", 1024 << m_superblock.log_block_size);
		dprintln("  inode size    {}", m_superblock.inode_size);
		dprintln("  inodes/group  {}", m_superblock.inodes_per_group);
#endif

		TRY(m_buffer_manager.initialize(block_size()));

		{
			auto block_buffer = m_buffer_manager.get_buffer();

			if (superblock().rev_level == Ext2::Enum::GOOD_OLD_REV)
			{
				// In revision 0 all blockgroups contain superblock backup
				TRY(m_superblock_backups.reserve(number_of_block_groups - 1));
				for (uint32_t i = 1; i < number_of_block_groups; i++)
					MUST(m_superblock_backups.push_back(i));
			}
			else
			{
				// In other revision superblock backups are on blocks 1 and powers of 3, 5 and 7
				TRY(m_superblock_backups.push_back(1));
				for (uint32_t i = 3; i < number_of_block_groups; i *= 3)
					TRY(m_superblock_backups.push_back(i));
				for (uint32_t i = 5; i < number_of_block_groups; i *= 5)
					TRY(m_superblock_backups.push_back(i));
				for (uint32_t i = 7; i < number_of_block_groups; i *= 7)
					TRY(m_superblock_backups.push_back(i));

				// We don't really care if this succeeds or not
				(void)m_superblock_backups.shrink_to_fit();
			}

			for (uint32_t bg : m_superblock_backups)
			{
				read_block(superblock().first_data_block + superblock().blocks_per_group * bg, block_buffer);
				Ext2::Superblock& superblock_backup = *(Ext2::Superblock*)block_buffer.data();
				if (superblock_backup.magic != Ext2::Enum::SUPER_MAGIC)
					derrorln("superblock backup at block {} is invalid ({4H})", bg, superblock_backup.magic);
			}
		}

		return {};
	}

	BAN::ErrorOr<void> Ext2FS::initialize_root_inode()
	{
		m_root_inode = TRY(Ext2Inode::create(*this, Ext2::Enum::ROOT_INO));
		return {};
	}

	BAN::ErrorOr<uint32_t> Ext2FS::create_inode(const Ext2::Inode& ext2_inode)
	{
		LockGuard _(m_lock);

		ASSERT(ext2_inode.size == 0);

		if (m_superblock.free_inodes_count == 0)
			return BAN::Error::from_errno(ENOSPC);

		const uint32_t block_size = this->block_size();

		auto bgd_buffer = m_buffer_manager.get_buffer();
		auto inode_bitmap = m_buffer_manager.get_buffer();

		uint32_t current_group = -1;
		BlockLocation bgd_location {};
		Ext2::BlockGroupDescriptor* bgd = nullptr;

		for (uint32_t ino = superblock().first_ino; ino <= superblock().inodes_count; ino++)
		{
			const uint32_t ino_group = (ino - 1) / superblock().inodes_per_group;
			const uint32_t ino_index = (ino - 1) % superblock().inodes_per_group;

			if (ino_group != current_group)
			{
				current_group = ino_group;

				bgd_location = locate_block_group_descriptior(current_group);
				read_block(bgd_location.block, bgd_buffer);

				bgd = (Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);
				if (bgd->free_inodes_count == 0)
				{
					ino = superblock().first_ino + (current_group + 1) * superblock().inodes_per_group - 1;
					continue;
				}

				read_block(bgd->inode_bitmap, inode_bitmap);
			}

			const uint32_t ino_bitmap_byte = ino_index / 8;
			const uint32_t ino_bitmap_bit  = ino_index % 8;
			if (inode_bitmap[ino_bitmap_byte] & (1 << ino_bitmap_bit))
				continue;

			inode_bitmap[ino_bitmap_byte] |= 1 << ino_bitmap_bit;
			write_block(bgd->inode_bitmap, inode_bitmap);

			bgd->free_inodes_count--;
			write_block(bgd_location.block, bgd_buffer);

			const uint32_t inode_table_offset = ino_index * superblock().inode_size;
			const BlockLocation inode_location
			{
				.block  = inode_table_offset / block_size + bgd->inode_table,
				.offset = inode_table_offset % block_size
			};

			// NOTE: we don't need inode bitmap anymore, so we can reuse it
			auto& inode_buffer = inode_bitmap;

			read_block(inode_location.block, inode_buffer);
			memcpy(inode_buffer.data() + inode_location.offset, &ext2_inode, sizeof(Ext2::Inode));
			if (superblock().inode_size > sizeof(Ext2::Inode))
				memset(inode_buffer.data() + inode_location.offset + sizeof(Ext2::Inode), 0, superblock().inode_size - sizeof(Ext2::Inode));
			write_block(inode_location.block, inode_buffer);

			m_superblock.free_inodes_count--;
			sync_superblock();

			return ino;
		}

		derrorln("Corrupted file system. Superblock indicates free inodes but none were found.");
		return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);
	}

	void Ext2FS::delete_inode(uint32_t ino)
	{
		LockGuard _(m_lock);

		ASSERT(ino <= superblock().inodes_count);

		auto bgd_buffer = get_block_buffer();
		auto bitmap_buffer = get_block_buffer();

		const uint32_t inode_group = (ino - 1) / superblock().inodes_per_group;
		const uint32_t inode_index = (ino - 1) % superblock().inodes_per_group;

		auto bgd_location = locate_block_group_descriptior(inode_group);
		read_block(bgd_location.block, bgd_buffer);

		auto& bgd = bgd_buffer.span().slice(bgd_location.offset).as<Ext2::BlockGroupDescriptor>();
		read_block(bgd.inode_bitmap, bitmap_buffer);

		const uint32_t byte = inode_index / 8;
		const uint32_t bit = inode_index % 8;
		ASSERT(bitmap_buffer[byte] & (1 << bit));

		bitmap_buffer[byte] &= ~(1 << bit);
		write_block(bgd.inode_bitmap, bitmap_buffer);

		bgd.free_inodes_count++;
		write_block(bgd_location.block, bgd_buffer);

		if (m_inode_cache.contains(ino))
			m_inode_cache.remove(ino);

		dprintln("succesfully deleted inode {}", ino);
	}

	void Ext2FS::read_block(uint32_t block, BlockBufferWrapper& buffer)
	{
		LockGuard _(m_lock);

		const uint32_t sector_size = m_partition.device().sector_size();
		const uint32_t block_size = this->block_size();
		const uint32_t sectors_per_block = block_size / sector_size;
		const uint32_t sectors_before = 2048 / sector_size;

		ASSERT(block >= 2);
		ASSERT(buffer.size() >= block_size);
		MUST(m_partition.read_sectors(sectors_before + (block - 2) * sectors_per_block, sectors_per_block, buffer.span()));
	}

	void Ext2FS::write_block(uint32_t block, const BlockBufferWrapper& buffer)
	{
		LockGuard _(m_lock);

		const uint32_t sector_size = m_partition.device().sector_size();
		const uint32_t block_size = this->block_size();
		const uint32_t sectors_per_block = block_size / sector_size;
		const uint32_t sectors_before = 2048 / sector_size;

		ASSERT(block >= 2);
		ASSERT(buffer.size() >= block_size);
		MUST(m_partition.write_sectors(sectors_before + (block - 2) * sectors_per_block, sectors_per_block, buffer.span()));
	}

	void Ext2FS::sync_superblock()
	{
		LockGuard _(m_lock);

		const uint32_t sector_size = m_partition.device().sector_size();
		ASSERT(1024 % sector_size == 0);

		const uint32_t superblock_bytes =
			(m_superblock.rev_level == Ext2::Enum::GOOD_OLD_REV)
				? offsetof(Ext2::Superblock, __extension_start)
				: sizeof(Ext2::Superblock);

		const uint32_t lba = 1024 / sector_size;
		const uint32_t sector_count = BAN::Math::div_round_up<uint32_t>(superblock_bytes, sector_size);

		auto superblock_buffer = get_block_buffer();

		MUST(m_partition.read_sectors(lba, sector_count, superblock_buffer.span()));
		if (memcmp(superblock_buffer.data(), &m_superblock, superblock_bytes))
		{
			memcpy(superblock_buffer.data(), &m_superblock, superblock_bytes);
			MUST(m_partition.write_sectors(lba, sector_count, superblock_buffer.span()));
		}
	}

	Ext2FS::BlockBufferWrapper Ext2FS::get_block_buffer()
	{
		LockGuard _(m_lock);
		return m_buffer_manager.get_buffer();
	}

	BAN::ErrorOr<uint32_t> Ext2FS::reserve_free_block(uint32_t primary_bgd)
	{
		LockGuard _(m_lock);

		if (m_superblock.r_blocks_count >= m_superblock.free_blocks_count)
			return BAN::Error::from_errno(ENOSPC);

		auto bgd_buffer = m_buffer_manager.get_buffer();
		auto block_bitmap = m_buffer_manager.get_buffer();

		auto check_block_group =
			[&](uint32_t block_group) -> uint32_t
			{
				auto bgd_location = locate_block_group_descriptior(block_group);
				read_block(bgd_location.block, bgd_buffer);

				auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);
				if (bgd.free_blocks_count == 0)
					return 0;

				read_block(bgd.block_bitmap, block_bitmap);
				for (uint32_t block_offset = 0; block_offset < m_superblock.blocks_per_group; block_offset++)
				{
					const uint32_t fs_block_index = m_superblock.first_data_block + m_superblock.blocks_per_group * block_group + block_offset;
					if (fs_block_index >= m_superblock.blocks_count)
						break;

					uint32_t byte = block_offset / 8;
					uint32_t bit  = block_offset % 8;
					if (block_bitmap[byte] & (1 << bit))
						continue;

					block_bitmap[byte] |= 1 << bit;
					write_block(bgd.block_bitmap, block_bitmap);

					bgd.free_blocks_count--;
					write_block(bgd_location.block, bgd_buffer);

					m_superblock.free_blocks_count--;
					sync_superblock();

					return fs_block_index;
				}

				derrorln("Corrupted file system. Block group descriptor indicates free blocks but none were found");
				return 0;
			};

		if (auto ret = check_block_group(primary_bgd))
			return ret;

		uint32_t number_of_block_groups = BAN::Math::div_round_up(m_superblock.blocks_count, m_superblock.blocks_per_group);
		for (uint32_t block_group = 0; block_group < number_of_block_groups; block_group++)
			if (block_group != primary_bgd)
				if (auto ret = check_block_group(block_group))
					return ret;

		derrorln("Corrupted file system. Superblock indicates free blocks but none were found.");
		return BAN::Error::from_error_code(ErrorCode::Ext2_Corrupted);
	}

	void Ext2FS::release_block(uint32_t block)
	{
		LockGuard _(m_lock);

		ASSERT(block >= m_superblock.first_data_block);
		ASSERT(block < m_superblock.blocks_count);

		const uint32_t block_group = (block - m_superblock.first_data_block) / m_superblock.blocks_per_group;
		const uint32_t block_offset = (block - m_superblock.first_data_block) % m_superblock.blocks_per_group;

		auto bgd_buffer = get_block_buffer();
		auto bitmap_buffer = get_block_buffer();

		auto bgd_location = locate_block_group_descriptior(block_group);
		read_block(bgd_location.block, bgd_buffer);

		auto& bgd = bgd_buffer.span().slice(bgd_location.offset).as<Ext2::BlockGroupDescriptor>();
		read_block(bgd.block_bitmap, bitmap_buffer);

		const uint32_t byte = block_offset / 8;
		const uint32_t bit  = block_offset % 8;
		ASSERT(bitmap_buffer[byte] & (1 << bit));

		bitmap_buffer[byte] &= ~(1 << bit);
		write_block(bgd.block_bitmap, bitmap_buffer);

		bgd.free_blocks_count++;
		write_block(bgd_location.block, bgd_buffer);

		dprintln("successfully freed block {}", block);
	}

	Ext2FS::BlockLocation Ext2FS::locate_inode(uint32_t ino)
	{
		LockGuard _(m_lock);

		ASSERT(ino <= superblock().inodes_count);

		const uint32_t block_size = this->block_size();

		auto bgd_buffer = m_buffer_manager.get_buffer();

		const uint32_t inode_group = (ino - 1) / superblock().inodes_per_group;
		const uint32_t inode_index = (ino - 1) % superblock().inodes_per_group;

		auto bgd_location = locate_block_group_descriptior(inode_group);

		read_block(bgd_location.block, bgd_buffer);

		auto& bgd = *(Ext2::BlockGroupDescriptor*)(bgd_buffer.data() + bgd_location.offset);

		const uint32_t inode_byte_offset = inode_index * superblock().inode_size;
		BlockLocation location
		{
			.block  = inode_byte_offset / block_size + bgd.inode_table,
			.offset = inode_byte_offset % block_size
		};

#if EXT2_VERIFY_INODE
		const uint32_t inode_bitmap_block = bgd.inode_bitmap;

		// NOTE: we can reuse the bgd_buffer since it is not needed anymore
		auto& inode_bitmap = bgd_buffer;

		read_block(inode_bitmap_block, inode_bitmap.span());

		const uint32_t byte = inode_index / 8;
		const uint32_t bit  = inode_index % 8;
		ASSERT(inode_bitmap[byte] & (1 << bit));
#endif

		return location;
	}

	Ext2FS::BlockLocation Ext2FS::locate_block_group_descriptior(uint32_t group_index)
	{
		LockGuard _(m_lock);

		const uint32_t block_size = this->block_size();

		const uint32_t block_group_count = BAN::Math::div_round_up(superblock().inodes_count, superblock().inodes_per_group);
		ASSERT(group_index < block_group_count);

		// Block Group Descriptor table is always after the superblock
		// Superblock begins at byte 1024 and is exactly 1024 bytes wide
		const uint32_t bgd_byte_offset = 2048 + sizeof(Ext2::BlockGroupDescriptor) * group_index;

		return
		{
			.block  = bgd_byte_offset / block_size,
			.offset = bgd_byte_offset % block_size
		};
	}

	Ext2FS::BlockBufferWrapper Ext2FS::BlockBufferManager::get_buffer()
	{
		for (auto& buffer : m_buffers)
		{
			if (buffer.used)
				continue;
			buffer.used = true;
			return Ext2FS::BlockBufferWrapper(buffer.buffer.span(), buffer.used);
		}
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Ext2FS::BlockBufferManager::initialize(size_t block_size)
	{
		for (auto& buffer : m_buffers)
		{
			TRY(buffer.buffer.resize(block_size));
			buffer.used = false;
		}
		return {};
	}

}