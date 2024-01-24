#pragma once

#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Process.h>

namespace Kernel
{

	class ProcFileSystem final : public TmpFileSystem
	{
	public:
		static void initialize();
		static ProcFileSystem& get();

		BAN::ErrorOr<void> on_process_create(Process&);
		void on_process_delete(Process&);

	private:
		ProcFileSystem();
	};

}
