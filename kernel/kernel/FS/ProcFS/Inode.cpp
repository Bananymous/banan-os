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

	BAN::ErrorOr<BAN::RefPtr<ProcMemInode>> ProcMemInode::create(Process& process, RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		FullInodeInfo inode_info(fs, mode, uid, gid);

		auto* inode_ptr = new ProcMemInode(process, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcMemInode>::adopt(inode_ptr);
	}

	ProcMemInode::ProcMemInode(Process& process, RamFileSystem& fs, const FullInodeInfo& inode_info)
		: RamInode(fs, inode_info)
		, m_process(process)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> ProcMemInode::read_impl(off_t offset, void* buffer, size_t buffer_size)
	{
		ASSERT(offset >= 0);
		if ((size_t)offset >= sizeof(proc_meminfo_t))
			return 0;

		proc_meminfo_t meminfo;
		m_process.get_meminfo(&meminfo);

		size_t bytes = BAN::Math::min<size_t>(buffer_size, sizeof(meminfo) - offset);
		memcpy(buffer, &meminfo, bytes);
		
		return bytes;
	}

}
