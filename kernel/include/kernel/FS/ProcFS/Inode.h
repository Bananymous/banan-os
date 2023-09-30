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

	class ProcMemInode final : public RamInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcMemInode>> create(Process&, RamFileSystem&, mode_t, uid_t, gid_t);
		~ProcMemInode() = default;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, void*, size_t) override;

		// You may not write here and this is always non blocking
		virtual BAN::ErrorOr<size_t> write_impl(off_t, const void*, size_t)	override	{ return BAN::Error::from_errno(EINVAL); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t) override						{ return BAN::Error::from_errno(EINVAL); }
		virtual bool has_data_impl() const override										{ return true; }

	private:
		ProcMemInode(Process&, RamFileSystem&, const FullInodeInfo&);
	
	private:
		Process& m_process;
	};

}
