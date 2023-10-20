#pragma once

#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/Process.h>

namespace Kernel
{

	class ProcPidInode final : public RamDirectoryInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcPidInode>> create(Process&, RamFileSystem&, mode_t, uid_t, gid_t);
		~ProcPidInode() = default;

	private:
		ProcPidInode(Process&, RamFileSystem&, const FullInodeInfo&);

	private:
		Process& m_process;
	};

	class ProcROInode final : public RamInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcROInode>> create(Process&, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, RamFileSystem&, mode_t, uid_t, gid_t);
		~ProcROInode() = default;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		// You may not write here and this is always non blocking
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan)	override	{ return BAN::Error::from_errno(EINVAL); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t) override						{ return BAN::Error::from_errno(EINVAL); }
		virtual bool has_data_impl() const override										{ return true; }

	private:
		ProcROInode(Process&, size_t (Process::*)(off_t, BAN::ByteSpan) const, RamFileSystem&, const FullInodeInfo&);
	
	private:
		Process& m_process;
		size_t (Process::*m_callback)(off_t, BAN::ByteSpan) const;
	};

}
