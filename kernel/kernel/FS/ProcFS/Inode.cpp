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

		TRY(inode->add_inode("meminfo"sv, MUST(ProcROInode::create(process, &Process::proc_meminfo, fs, 0755, 0, 0))));
		TRY(inode->add_inode("cmdline"sv, MUST(ProcROInode::create(process, &Process::proc_cmdline, fs, 0755, 0, 0))));
		TRY(inode->add_inode("environ"sv, MUST(ProcROInode::create(process, &Process::proc_environ, fs, 0755, 0, 0))));

		return inode;
	}

	ProcPidInode::ProcPidInode(Process& process, RamFileSystem& fs, const FullInodeInfo& inode_info)
		: RamDirectoryInode(fs, inode_info, fs.root_inode()->ino())
		, m_process(process)
	{
	}

	BAN::ErrorOr<BAN::RefPtr<ProcROInode>> ProcROInode::create(Process& process, size_t (Process::*callback)(off_t, void*, size_t) const, RamFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		FullInodeInfo inode_info(fs, mode, uid, gid);

		auto* inode_ptr = new ProcROInode(process, callback, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcROInode>::adopt(inode_ptr);
	}

	ProcROInode::ProcROInode(Process& process, size_t (Process::*callback)(off_t, void*, size_t) const, RamFileSystem& fs, const FullInodeInfo& inode_info)
		: RamInode(fs, inode_info)
		, m_process(process)
		, m_callback(callback)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> ProcROInode::read_impl(off_t offset, void* buffer, size_t buffer_size)
	{
		ASSERT(offset >= 0);
		if ((size_t)offset >= sizeof(proc_meminfo_t))
			return 0;
		return (m_process.*m_callback)(offset, buffer, buffer_size);
	}

}
