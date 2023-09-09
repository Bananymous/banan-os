#pragma once

#include <BAN/HashMap.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class RamInode;
	class RamDirectoryInode;

	class RamFileSystem : public FileSystem
	{
	public:
		static BAN::ErrorOr<RamFileSystem*> create(size_t size, mode_t, uid_t, gid_t);
		virtual ~RamFileSystem() = default;

		BAN::ErrorOr<void> set_root_inode(BAN::RefPtr<RamDirectoryInode>);
		virtual BAN::RefPtr<Inode> root_inode() override { return m_inodes[m_root_inode]; }

		BAN::ErrorOr<void> add_inode(BAN::RefPtr<RamInode>);
		BAN::ErrorOr<BAN::RefPtr<RamInode>> get_inode(ino_t);

		blksize_t blksize() const { return m_blksize; }
		ino_t next_ino() { return m_next_ino++; }

		void for_each_inode(void (*callback)(BAN::RefPtr<RamInode>));

	protected:
		RamFileSystem(size_t size)
			: m_size(size)
		{ }

	private:
		RecursiveSpinLock m_lock;
		size_t m_size { 0 };

		BAN::HashMap<ino_t, BAN::RefPtr<RamInode>> m_inodes;
		ino_t m_root_inode { 0 };

		const blksize_t m_blksize = PAGE_SIZE;
		ino_t m_next_ino { 1 };
	};

}