#pragma once

#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>

namespace Kernel
{

	class Process;

	class ProcFileSystem final : public TmpFileSystem
	{
	public:
		static void initialize();
		static ProcFileSystem& get();

		void post_scheduler_initialize();

		BAN::ErrorOr<void> on_process_create(Process&);
		void on_process_delete(Process&);

	private:
		ProcFileSystem();
	};

}
