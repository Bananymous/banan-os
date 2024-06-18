#include <kernel/FS/ProcFS/Inode.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<ProcPidInode>> ProcPidInode::create_new(Process& process, TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto inode_info = create_inode_info(Mode::IFDIR | mode, uid, gid);

		auto* inode_ptr = new ProcPidInode(process, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto inode = BAN::RefPtr<ProcPidInode>::adopt(inode_ptr);

		TRY(inode->link_inode(*MUST(ProcROInode::create_new(process, &Process::proc_meminfo, fs, 0400, uid, gid)), "meminfo"_sv));
		TRY(inode->link_inode(*MUST(ProcROInode::create_new(process, &Process::proc_cmdline, fs, 0400, uid, gid)), "cmdline"_sv));
		TRY(inode->link_inode(*MUST(ProcROInode::create_new(process, &Process::proc_environ, fs, 0400, uid, gid)), "environ"_sv));

		return inode;
	}

	ProcPidInode::ProcPidInode(Process& process, TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpDirectoryInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_process(process)
	{
	}

	void ProcPidInode::cleanup()
	{
		(void)TmpDirectoryInode::unlink_impl("meminfo"_sv);
		(void)TmpDirectoryInode::unlink_impl("cmdline"_sv);
		(void)TmpDirectoryInode::unlink_impl("environ"_sv);
	}

	BAN::ErrorOr<BAN::RefPtr<ProcROInode>> ProcROInode::create_new(Process& process, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto inode_info = create_inode_info(Mode::IFREG | mode, uid, gid);

		auto* inode_ptr = new ProcROInode(process, callback, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcROInode>::adopt(inode_ptr);
	}

	ProcROInode::ProcROInode(Process& process, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_process(process)
		, m_callback(callback)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> ProcROInode::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		ASSERT(offset >= 0);
		if ((size_t)offset >= sizeof(proc_meminfo_t))
			return 0;
		return (m_process.*m_callback)(offset, buffer);
	}

}
