#include <BAN/ScopeGuard.h>
#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/LockGuard.h>
#include <kernel/RTC.h>

namespace Kernel
{

	BAN::ErrorOr<RamFileSystem*> RamFileSystem::create(size_t size, mode_t mode, uid_t uid, gid_t gid)
	{
		auto* ramfs = new RamFileSystem;
		if (ramfs == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		ramfs->m_size = size;

		BAN::ScopeGuard deleter([ramfs] { delete ramfs; });

		auto root_inode = TRY(RamDirectoryInode::create(*ramfs, 0, mode, uid, gid));
		TRY(ramfs->add_inode(root_inode));
		ramfs->m_root_inode = root_inode->ino();

		deleter.disable();

		return ramfs;
	}

	BAN::ErrorOr<void> RamFileSystem::add_inode(BAN::RefPtr<RamInode> inode)
	{
		LockGuard _(m_lock);
		if (m_inodes.contains(inode->ino()))
			return BAN::Error::from_errno(EEXIST);
		TRY(m_inodes.insert(inode->ino(), inode));
		return {};
	}

	BAN::ErrorOr<BAN::RefPtr<RamInode>> RamFileSystem::get_inode(ino_t ino)
	{
		LockGuard _(m_lock);
		if (!m_inodes.contains(ino))
			return BAN::Error::from_errno(ENOENT);
		return m_inodes[ino];
	}

}