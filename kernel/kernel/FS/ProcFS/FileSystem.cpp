#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/ProcFS/Inode.h>
#include <kernel/Lock/LockGuard.h>

namespace Kernel
{

	static ProcFileSystem* s_instance = nullptr;

	void ProcFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new ProcFileSystem();
		ASSERT(s_instance);

		MUST(s_instance->TmpFileSystem::initialize(0555, 0, 0));
	}

	ProcFileSystem& ProcFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	ProcFileSystem::ProcFileSystem()
		: TmpFileSystem(-1)
	{
	}

	BAN::ErrorOr<void> ProcFileSystem::on_process_create(Process& process)
	{
		auto path = TRY(BAN::String::formatted("{}", process.pid()));
		auto inode = TRY(ProcPidInode::create_new(process, *this, 0555, process.credentials().ruid(), process.credentials().rgid()));
		TRY(static_cast<TmpDirectoryInode*>(root_inode().ptr())->link_inode(*inode, path));
		return {};
	}

	void ProcFileSystem::on_process_delete(Process& process)
	{
		auto path = MUST(BAN::String::formatted("{}", process.pid()));

		auto inode = MUST(root_inode()->find_inode(path));
		static_cast<ProcPidInode*>(inode.ptr())->cleanup();

		if (auto ret = root_inode()->unlink(path); ret.is_error())
			dwarnln("{}", ret.error());
	}

}
