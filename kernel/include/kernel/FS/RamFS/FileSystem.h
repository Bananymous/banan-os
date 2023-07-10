#pragma once

#include <BAN/HashMap.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/SpinLock.h>

namespace Kernel
{
	
	class RamInode;

	class RamFileSystem final : public FileSystem
	{
	public:
		static BAN::ErrorOr<RamFileSystem*> create(size_t size, mode_t, uid_t, gid_t);
		~RamFileSystem() = default;

		virtual BAN::RefPtr<Inode> root_inode() override { return m_inodes[m_root_inode]; }

		BAN::ErrorOr<void> add_inode(BAN::RefPtr<RamInode>);
		BAN::ErrorOr<BAN::RefPtr<RamInode>> get_inode(ino_t);

		blksize_t blksize() const { return m_blksize; }
		ino_t next_ino() { return m_next_ino++; }

	private:
		RamFileSystem() = default;

	private:
		SpinLock m_lock;
		size_t m_size { 0 };

		BAN::HashMap<ino_t, BAN::RefPtr<RamInode>> m_inodes;
		ino_t m_root_inode;

		const blksize_t m_blksize = PAGE_SIZE;
		ino_t m_next_ino { 1 };
	};

}