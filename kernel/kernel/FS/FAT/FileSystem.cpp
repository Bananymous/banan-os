#include <kernel/FS/FAT/FileSystem.h>
#include <kernel/Lock/LockGuard.h>

#include <ctype.h>

namespace Kernel
{

	bool FATFS::validate_bpb(const FAT::BPB& bpb)
	{
		bool valid_jump_op = (bpb.jump_op[0] == 0xEB && bpb.jump_op[2] == 0x90) || (bpb.jump_op[0] == 0xE9);
		if (!valid_jump_op)
			return false;

		// This is techincally a strict requirement
		for (char c : bpb.oem_name)
			if (!isprint(c))
				return false;

		if (!BAN::Math::is_power_of_two(bpb.bytes_per_sector) || bpb.bytes_per_sector < 512 || bpb.bytes_per_sector > 4096)
			return false;

		if (!BAN::Math::is_power_of_two(bpb.sectors_per_cluster) || bpb.sectors_per_cluster > 128)
			return false;

		if (bpb.reserved_sector_count == 0)
			return false;

		if (bpb.number_of_fats == 0)
			return false;

		switch (bpb.media_type)
		{
			case 0xF0:
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
			case 0xFC:
			case 0xFD:
			case 0xFE:
			case 0xFF:
				break;
			default:
				return false;
		}

		// FIXME: There is more possible checks to do

		return true;
	}

	BAN::ErrorOr<bool> FATFS::probe(BAN::RefPtr<BlockDevice> block_device)
	{
		// support only block devices with sectors at least 512 bytes
		if (block_device->blksize() < 512)
			return false;

		BAN::Vector<uint8_t> bpb_buffer;
		TRY(bpb_buffer.resize(block_device->blksize()));
		auto bpb_span = BAN::ByteSpan(bpb_buffer.span());
		TRY(block_device->read_blocks(0, 1, bpb_span));

		return validate_bpb(bpb_span.as<const FAT::BPB>());
	}

	unsigned long FATFS::bsize()   const { return m_bpb.bytes_per_sector; }
	unsigned long FATFS::frsize()  const { return m_bpb.bytes_per_sector; }
	fsblkcnt_t    FATFS::blocks()  const { return data_sector_count(); }
	fsblkcnt_t    FATFS::bfree()   const { return 0; } // FIXME
	fsblkcnt_t    FATFS::bavail()  const { return 0; } // FIXME
	fsfilcnt_t    FATFS::files()   const { return m_bpb.number_of_fats * fat_size() * 8 / bits_per_fat_entry(); }
	fsfilcnt_t    FATFS::ffree()   const { return 0; } // FIXME
	fsfilcnt_t    FATFS::favail()  const { return 0; } // FIXME
	unsigned long FATFS::fsid()    const { return m_type == Type::FAT32 ? m_bpb.ext_32.volume_id : m_bpb.ext_12_16.volume_id; }
	unsigned long FATFS::flag()    const { return 0; }
	unsigned long FATFS::namemax() const { return 255; }

	BAN::ErrorOr<BAN::RefPtr<FATFS>> FATFS::create(BAN::RefPtr<BlockDevice> block_device)
	{
		// support only block devices with sectors at least 512 bytes
		if (block_device->blksize() < 512)
			return BAN::Error::from_errno(EINVAL);

		BAN::Vector<uint8_t> bpb_buffer;
		TRY(bpb_buffer.resize(block_device->blksize()));
		auto bpb_span = BAN::ByteSpan(bpb_buffer.span());
		TRY(block_device->read_blocks(0, 1, bpb_span));

		const auto& bpb = bpb_span.as<const FAT::BPB>();
		if (!validate_bpb(bpb))
			return BAN::Error::from_errno(EINVAL);

		auto fatfs = TRY(BAN::RefPtr<FATFS>::create(
			block_device,
			bpb
		));
		TRY(fatfs->initialize());
		return fatfs;
	}

	BAN::ErrorOr<void> FATFS::initialize()
	{
		if (m_bpb.bytes_per_sector != m_block_device->blksize())
		{
			dwarnln("FileSystem sector size does not match with block device");
			return BAN::Error::from_errno(ENOTSUP);
		}

		TRY(m_fat_two_sector_buffer.resize(m_bpb.bytes_per_sector * 2));
		TRY(m_block_device->read_blocks(first_fat_sector(), 2, BAN::ByteSpan(m_fat_two_sector_buffer.span())));
		m_fat_sector_buffer_current = 0;

		FAT::DirectoryEntry root_entry {};
		root_entry.attr = FAT::FileAttr::DIRECTORY;
		m_root_inode = TRY(open_inode(nullptr, root_entry, 0, 0));

		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<FATInode>> FATFS::open_inode(BAN::RefPtr<FATInode> parent, const FAT::DirectoryEntry& entry, uint32_t cluster_index, uint32_t entry_index)
	{
		LockGuard _(m_mutex);

		uint32_t block_count = 0;
		{
			uint32_t cluster = entry.first_cluster_lo;
			if (m_type == Type::FAT32)
				cluster |= static_cast<uint32_t>(entry.first_cluster_hi) << 16;
			while (cluster >= 2 && cluster < cluster_count())
			{
				block_count++;
				cluster = TRY(get_next_cluster(cluster));
			}
		}

		uint32_t entry_cluster;
		switch (m_type)
		{
			case Type::FAT12:
			case Type::FAT16:
				if (parent == m_root_inode)
					entry_cluster = 1;
				else
				{
					entry_cluster = parent->entry().first_cluster_lo;
					for (uint32_t i = 0; i < cluster_index; i++)
						entry_cluster = TRY(get_next_cluster(entry_cluster));
				}
				break;
			case Type::FAT32:
				if (parent == m_root_inode)
					entry_cluster = m_bpb.ext_32.root_cluster;
				else
					entry_cluster = (static_cast<uint32_t>(parent->entry().first_cluster_hi) << 16) | parent->entry().first_cluster_lo;
				for (uint32_t i = 0; i < cluster_index; i++)
					entry_cluster = TRY(get_next_cluster(entry_cluster));
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		const ino_t ino = (static_cast<ino_t>(entry_cluster) << 32) | entry_index;
		auto it = m_inode_cache.find(ino);
		if (it != m_inode_cache.end())
		{
			if (auto inode = it->value.lock())
				return inode;
			m_inode_cache.remove(it);
		}

		auto inode = TRY(BAN::RefPtr<FATInode>::create(*this, entry, ino, block_count));
		TRY(m_inode_cache.insert(ino, TRY(inode->get_weak_ptr())));
		return inode;
	}

	BAN::ErrorOr<void> FATFS::fat_cache_set_sector(uint32_t sector)
	{
		if (m_fat_sector_buffer_current != sector)
		{
			TRY(m_block_device->read_blocks(first_fat_sector() + sector, 2, BAN::ByteSpan(m_fat_two_sector_buffer.span())));
			m_fat_sector_buffer_current = sector;
		}
		return {};
	}

	BAN::ErrorOr<uint32_t> FATFS::get_next_cluster(uint32_t cluster)
	{
		LockGuard _(m_mutex);

		ASSERT(cluster >= 2 && cluster < cluster_count());

		auto fat_span = BAN::ByteSpan(m_fat_two_sector_buffer.span());

		switch (m_type)
		{
			case Type::FAT12:
			{
				const uint32_t fat_byte_offset = cluster + (cluster / 2);
				const uint32_t ent_offset = fat_byte_offset % m_bpb.bytes_per_sector;
				TRY(fat_cache_set_sector(fat_byte_offset / m_bpb.bytes_per_sector));
				uint16_t next = (fat_span[ent_offset + 1] << 8) | fat_span[ent_offset];
				return cluster % 2 ? next >> 4 : next & 0xFFF;
			}
			case Type::FAT16:
			{
				const uint32_t fat_byte_offset = cluster * sizeof(uint16_t);
				const uint32_t ent_offset = (fat_byte_offset % m_bpb.bytes_per_sector) / sizeof(uint16_t);
				TRY(fat_cache_set_sector(fat_byte_offset / m_bpb.bytes_per_sector));
				return fat_span.as_span<uint16_t>()[ent_offset];
			}
			case Type::FAT32:
			{
				const uint32_t fat_byte_offset = cluster * sizeof(uint32_t);
				const uint32_t ent_offset = (fat_byte_offset % m_bpb.bytes_per_sector) / sizeof(uint32_t);
				TRY(fat_cache_set_sector(fat_byte_offset / m_bpb.bytes_per_sector));
				return fat_span.as_span<uint32_t>()[ent_offset];
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> FATFS::inode_read_cluster(BAN::RefPtr<FATInode> file, size_t index, BAN::ByteSpan buffer)
	{
		LockGuard _(m_mutex);

		if (buffer.size() < static_cast<BAN::make_unsigned_t<decltype(file->blksize())>>(file->blksize()))
			return BAN::Error::from_errno(ENOBUFS);

		uint32_t cluster;
		switch (m_type)
		{
			case Type::FAT12:
			case Type::FAT16:
			{
				if (file == m_root_inode)
				{
					if (index >= root_sector_count())
						return BAN::Error::from_errno(ENOENT);
					const uint32_t first_root_sector = m_bpb.reserved_sector_count + (m_bpb.number_of_fats * fat_size());
					TRY(m_block_device->read_blocks(first_root_sector + index, 1, buffer));
					return {};
				}
				cluster = file->entry().first_cluster_lo;
				break;
			}
			case Type::FAT32:
				if (file == m_root_inode)
					cluster = m_bpb.ext_32.root_cluster;
				else
					cluster = (static_cast<uint32_t>(file->entry().first_cluster_hi) << 16) | file->entry().first_cluster_lo;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		if (cluster < 2 || cluster >= cluster_count())
			return BAN::Error::from_errno(ENOENT);

		for (uint32_t i = 0; i < index; i++)
		{
			cluster = TRY(get_next_cluster(cluster));
			if (cluster < 2 || cluster >= cluster_count())
				return BAN::Error::from_errno(ENOENT);
		}

		const uint32_t cluster_start_sector = ((cluster - 2) * m_bpb.sectors_per_cluster) + first_data_sector();
		TRY(m_block_device->read_blocks(cluster_start_sector, m_bpb.sectors_per_cluster, buffer));
		return {};
	}

	blksize_t FATFS::inode_block_size(BAN::RefPtr<const FATInode> file) const
	{
		switch (m_type)
		{
			case Type::FAT12:
			case Type::FAT16:
				if (file == m_root_inode)
					return m_bpb.bytes_per_sector;
				return m_bpb.bytes_per_sector * m_bpb.sectors_per_cluster;
			case Type::FAT32:
				return m_bpb.bytes_per_sector * m_bpb.sectors_per_cluster;
		}
		ASSERT_NOT_REACHED();
	}

}
