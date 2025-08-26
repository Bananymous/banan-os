#include <kernel/BootInfo.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/ProcFS/Inode.h>

namespace Kernel
{

	static ProcFileSystem* s_instance = nullptr;

	void ProcFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new ProcFileSystem();
		ASSERT(s_instance);

		MUST(s_instance->TmpFileSystem::initialize(0555, 0, 0));

		auto meminfo_inode = MUST(ProcROInode::create_new(
			[](off_t offset, BAN::ByteSpan buffer) -> size_t
			{
				ASSERT(offset >= 0);
				if ((size_t)offset >= sizeof(full_meminfo_t))
					return 0;

				full_meminfo_t meminfo;
				meminfo.page_size = PAGE_SIZE;
				meminfo.free_pages = Heap::get().free_pages();
				meminfo.used_pages = Heap::get().used_pages();

				size_t bytes = BAN::Math::min<size_t>(sizeof(full_meminfo_t) - offset, buffer.size());
				memcpy(buffer.data(), (uint8_t*)&meminfo + offset, bytes);
				return bytes;
			},
			*s_instance, 0444, 0, 0
		));
		MUST(static_cast<TmpDirectoryInode*>(s_instance->root_inode().ptr())->link_inode(*meminfo_inode, "meminfo"_sv));

		auto cmdline_inode = MUST(TmpFileInode::create_new(*s_instance, 0444, 0, 0));
		MUST(cmdline_inode->write(0, { reinterpret_cast<const uint8_t*>(g_boot_info.command_line.data()), g_boot_info.command_line.size() }));
		MUST(static_cast<TmpDirectoryInode*>(s_instance->root_inode().ptr())->link_inode(*cmdline_inode, "cmdline"_sv));

		auto self_inode = MUST(ProcSymlinkInode::create_new(
			[](void*) -> BAN::ErrorOr<BAN::String> {
				return BAN::String::formatted("{}", Process::current().pid());
			},
			nullptr, *s_instance, 0444, 0, 0)
		);
		MUST(static_cast<TmpDirectoryInode*>(s_instance->root_inode().ptr())->link_inode(*self_inode, "self"_sv));
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
		auto inode = TRY(ProcPidInode::create_new(process, *this, 0555));
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
