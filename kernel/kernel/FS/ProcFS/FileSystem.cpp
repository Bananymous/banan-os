#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/ProcFS/Inode.h>
#include <kernel/FS/RamFS/Inode.h>
#include <kernel/LockGuard.h>

namespace Kernel
{

	static ProcFileSystem* s_instance = nullptr;

	void ProcFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new ProcFileSystem(1024 * 1024);
		ASSERT(s_instance);

		s_instance->m_root_inode = MUST(RamDirectoryInode::create(*s_instance, 0, 0555, 0, 0));
		MUST(s_instance->set_root_inode(s_instance->m_root_inode));
	}

	ProcFileSystem& ProcFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	ProcFileSystem::ProcFileSystem(size_t size)
		: RamFileSystem(size)
	{
	}

	BAN::ErrorOr<void> ProcFileSystem::on_process_create(Process& process)
	{
		auto path = BAN::String::formatted("{}", process.pid());
		auto inode = TRY(ProcPidInode::create(process, *this, 0555, 0, 0));
		TRY(m_root_inode->add_inode(path, inode));
		return {};
	}

	void ProcFileSystem::on_process_delete(Process& process)
	{
		auto path = BAN::String::formatted("{}", process.pid());
		MUST(m_root_inode->unlink(path));
	}

}
