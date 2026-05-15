#pragma once

#include <BAN/Function.h>
#include <BAN/Iteration.h>
#include <BAN/WeakPtr.h>

#include <kernel/FS/FAT/Definitions.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{

	class FATFS;

	class FATInode final : public Inode, public BAN::Weakable<FATInode>
	{
	public:
		virtual const FileSystem* filesystem() const override;

		const FAT::DirectoryEntry& entry() const { return m_entry; }

	protected:
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> find_inode_impl(BAN::StringView) override;
		virtual BAN::ErrorOr<size_t> list_next_inodes_impl(off_t, struct dirent*, size_t) override;
		//virtual BAN::ErrorOr<void> create_file_impl(BAN::StringView, mode_t, uid_t, gid_t) override;
		//virtual BAN::ErrorOr<void> create_directory_impl(BAN::StringView, mode_t, uid_t, gid_t) override;
		//virtual BAN::ErrorOr<void> unlink_impl(BAN::StringView) override;

		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		//virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;
		//virtual BAN::ErrorOr<void> truncate_impl(size_t) override;
		//virtual BAN::ErrorOr<void> chmod_impl(mode_t) override;
		//virtual BAN::ErrorOr<void> utimens_impl(const timespec[2]) override;
		virtual BAN::ErrorOr<void> fsync_impl() override { return {}; }

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override { return false; }

	private:
		FATInode(FATFS& fs, const FAT::DirectoryEntry& entry, ino_t ino, uint32_t block_count);

		~FATInode()	{}

		BAN::ErrorOr<void> for_each_directory_entry(BAN::ConstByteSpan, BAN::Function<BAN::Iteration(const FAT::DirectoryEntry&)>);
		BAN::ErrorOr<void> for_each_directory_entry(BAN::ConstByteSpan, BAN::Function<BAN::Iteration(const FAT::DirectoryEntry&, BAN::String, uint32_t)>);

	private:
		FATFS& m_fs;
		FAT::DirectoryEntry m_entry;
		uint32_t m_block_count;

		friend class Ext2FS;
		friend class BAN::RefPtr<FATInode>;
	};

}
