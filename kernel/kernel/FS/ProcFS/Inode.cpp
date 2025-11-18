#include <kernel/FS/ProcFS/Inode.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<ProcPidInode>> ProcPidInode::create_new(Process& process, TmpFileSystem& fs, mode_t mode)
	{
		auto inode_info = create_inode_info(Mode::IFDIR | mode, 0, 0);

		auto* inode_ptr = new ProcPidInode(process, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto inode = BAN::RefPtr<ProcPidInode>::adopt(inode_ptr);

		TRY(inode->link_inode(*inode, "."_sv));
		TRY(inode->link_inode(static_cast<TmpInode&>(*fs.root_inode()), ".."_sv));
		TRY(inode->link_inode(*MUST(ProcROProcessInode::create_new(process, &Process::proc_meminfo, fs, 0400)), "meminfo"_sv));
		TRY(inode->link_inode(*MUST(ProcROProcessInode::create_new(process, &Process::proc_cmdline, fs, 0444)), "cmdline"_sv));
		TRY(inode->link_inode(*MUST(ProcROProcessInode::create_new(process, &Process::proc_environ, fs, 0400)), "environ"_sv));
		TRY(inode->link_inode(*MUST(ProcSymlinkProcessInode::create_new(process, &Process::proc_cwd, fs, 0777)), "cwd"_sv));
		TRY(inode->link_inode(*MUST(ProcSymlinkProcessInode::create_new(process, &Process::proc_exe, fs, 0777)), "exe"_sv));

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
		(void)TmpDirectoryInode::unlink_impl("cwd"_sv);
		(void)TmpDirectoryInode::unlink_impl("exe"_sv);
	}

	BAN::ErrorOr<BAN::RefPtr<ProcROProcessInode>> ProcROProcessInode::create_new(Process& process, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, TmpFileSystem& fs, mode_t mode)
	{
		auto inode_info = create_inode_info(Mode::IFREG | mode, 0, 0);

		auto* inode_ptr = new ProcROProcessInode(process, callback, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcROProcessInode>::adopt(inode_ptr);
	}

	ProcROProcessInode::ProcROProcessInode(Process& process, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_process(process)
		, m_callback(callback)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> ProcROProcessInode::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		if (offset < 0)
			return BAN::Error::from_errno(EINVAL);
		return (m_process.*m_callback)(offset, buffer);
	}

	BAN::ErrorOr<BAN::RefPtr<ProcSymlinkProcessInode>> ProcSymlinkProcessInode::create_new(Process& process, BAN::ErrorOr<BAN::String> (Process::*callback)() const, TmpFileSystem& fs, mode_t mode)
	{
		auto inode_info = create_inode_info(Mode::IFLNK | mode, 0, 0);

		auto* inode_ptr = new ProcSymlinkProcessInode(process, callback, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcSymlinkProcessInode>::adopt(inode_ptr);
	}

	ProcSymlinkProcessInode::ProcSymlinkProcessInode(Process& process, BAN::ErrorOr<BAN::String> (Process::*callback)() const, TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_process(process)
		, m_callback(callback)
	{
		m_inode_info.mode |= Inode::Mode::IFLNK;
	}

	BAN::ErrorOr<BAN::String> ProcSymlinkProcessInode::link_target_impl()
	{
		return (m_process.*m_callback)();
	}

	BAN::ErrorOr<BAN::RefPtr<ProcROInode>> ProcROInode::create_new(size_t (*callback)(off_t, BAN::ByteSpan), TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto inode_info = create_inode_info(Mode::IFREG | mode, uid, gid);

		auto* inode_ptr = new ProcROInode(callback, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcROInode>::adopt(inode_ptr);
	}

	ProcROInode::ProcROInode(size_t (*callback)(off_t, BAN::ByteSpan), TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_callback(callback)
	{
		m_inode_info.mode |= Inode::Mode::IFREG;
	}

	BAN::ErrorOr<size_t> ProcROInode::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		if (offset < 0)
			return BAN::Error::from_errno(EINVAL);
		return m_callback(offset, buffer);
	}

	BAN::ErrorOr<BAN::RefPtr<ProcSymlinkInode>> ProcSymlinkInode::create_new(BAN::ErrorOr<BAN::String> (*callback)(void*), void* data, TmpFileSystem& fs, mode_t mode, uid_t uid, gid_t gid)
	{
		auto inode_info = create_inode_info(Mode::IFLNK | mode, uid, gid);

		auto* inode_ptr = new ProcSymlinkInode(callback, data, fs, inode_info);
		if (inode_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ProcSymlinkInode>::adopt(inode_ptr);
	}

	ProcSymlinkInode::ProcSymlinkInode(BAN::ErrorOr<BAN::String> (*callback)(void*), void* data, TmpFileSystem& fs, const TmpInodeInfo& inode_info)
		: TmpInode(fs, MUST(fs.allocate_inode(inode_info)), inode_info)
		, m_callback(callback)
		, m_data(data)
	{
		m_inode_info.mode |= Inode::Mode::IFLNK;
	}

	BAN::ErrorOr<BAN::String> ProcSymlinkInode::link_target_impl()
	{
		return m_callback(m_data);
	}

}
