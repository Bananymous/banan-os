#pragma once

#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/Process.h>

namespace Kernel
{

	class ProcFileSystem final : public RamFileSystem
	{
	public:
		static void initialize();
		static ProcFileSystem& get();

		BAN::ErrorOr<void> on_process_create(Process&);
		void on_process_delete(Process&);

	private:
		ProcFileSystem(size_t size);

	private:
		BAN::RefPtr<RamDirectoryInode> m_root_inode;
	};

}