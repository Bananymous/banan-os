#include <kernel/FS/ProcFS/Inode.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<ProcPidInode>> ProcPidInode::create(Process& process, RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		FullInodeInfo inode_info(fs, mode, uid, gid);

		auto* inode_ptr = new ProcPidInode(process, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto inode = BAN::RefPtr<ProcPidInode>::adopt(inode_ptr);

		TRY(inode->add_inode("meminfo"sv, MUST(ProcMemInode::create(process, fs, 0755, 0, 0))));

		return inode;
	}

	ProcPidInode::ProcPidInode(Process& process, RamFileSystem& fs, const FullInodeInfo& inode_info)
		: RamDirectoryInode(fs, inode_info, fs.root_inode()->ino())
		, m_process(process)
	{
	}

}
