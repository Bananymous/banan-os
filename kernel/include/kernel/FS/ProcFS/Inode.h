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

}
