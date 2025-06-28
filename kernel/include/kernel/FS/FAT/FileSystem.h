#pragma once

#include <BAN/HashMap.h>

#include <kernel/FS/FAT/Definitions.h>
#include <kernel/FS/FAT/Inode.h>
#include <kernel/FS/FileSystem.h>

namespace Kernel
{

	class FATFS final : public FileSystem
	{
	public:
		enum class Type
		{
			FAT12 = 12,
			FAT16 = 16,
			FAT32 = 32,
		};

	public:
		virtual unsigned long bsize()   const override;
		virtual unsigned long frsize()  const override;
		virtual fsblkcnt_t    blocks()  const override;
		virtual fsblkcnt_t    bfree()   const override;
		virtual fsblkcnt_t    bavail()  const override;
		virtual fsfilcnt_t    files()   const override;
		virtual fsfilcnt_t    ffree()   const override;
		virtual fsfilcnt_t    favail()  const override;
		virtual unsigned long fsid()    const override;
		virtual unsigned long flag()    const override;
		virtual unsigned long namemax() const override;

		static BAN::ErrorOr<bool> probe(BAN::RefPtr<BlockDevice>);
		static BAN::ErrorOr<BAN::RefPtr<FATFS>> create(BAN::RefPtr<BlockDevice>);

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_inode; }

		BAN::ErrorOr<BAN::RefPtr<FATInode>> open_inode(BAN::RefPtr<FATInode> parent, const FAT::DirectoryEntry& entry, uint32_t cluster_index, uint32_t entry_index);
		BAN::ErrorOr<void> inode_read_cluster(BAN::RefPtr<FATInode>, size_t index, BAN::ByteSpan buffer);
		blksize_t inode_block_size(BAN::RefPtr<const FATInode>) const;

	private:
		FATFS(BAN::RefPtr<BlockDevice> block_device, FAT::BPB bpb)
			: m_block_device(block_device)
			, m_bpb(bpb)
			, m_type((cluster_count() < 4085) ? Type::FAT12 : (cluster_count() < 65525) ? Type::FAT16 : Type::FAT32)
		{}
		static bool validate_bpb(const FAT::BPB&);
		BAN::ErrorOr<void> initialize();

		BAN::ErrorOr<void> fat_cache_set_sector(uint32_t sector);
		BAN::ErrorOr<uint32_t> get_next_cluster(uint32_t cluster);

		// TODO: These probably should be constant variables
		uint32_t root_sector_count() const   { return BAN::Math::div_round_up<uint32_t>(m_bpb.root_entry_count * 32, m_bpb.bytes_per_sector); }
		uint32_t fat_size() const            { return m_bpb.fat_size16 ? m_bpb.fat_size16 : m_bpb.ext_32.fat_size32; }
		uint32_t total_sector_count() const { return m_bpb.total_sectors16 ? m_bpb.total_sectors16 : m_bpb.total_sectors32; }
		uint32_t data_sector_count() const  { return total_sector_count() - (m_bpb.reserved_sector_count + (m_bpb.number_of_fats * fat_size()) + root_sector_count()); }
		uint32_t cluster_count() const      { return data_sector_count() / m_bpb.sectors_per_cluster; }
		uint32_t first_data_sector() const  { return m_bpb.reserved_sector_count + (m_bpb.number_of_fats * fat_size()) + root_sector_count(); }
		uint32_t first_fat_sector() const   { return m_bpb.reserved_sector_count; }

		uint8_t bits_per_fat_entry() const  { return static_cast<uint8_t>(m_type); }

	private:
		BAN::RefPtr<BlockDevice> m_block_device;
		BAN::RefPtr<FATInode> m_root_inode;

		const FAT::BPB m_bpb;
		const Type m_type;

		BAN::HashMap<ino_t, BAN::WeakPtr<FATInode>> m_inode_cache;

		BAN::Vector<uint8_t> m_fat_two_sector_buffer;
		uint32_t m_fat_sector_buffer_current { 0 };

		Mutex m_mutex;

		friend class BAN::RefPtr<FATFS>;
	};

}
