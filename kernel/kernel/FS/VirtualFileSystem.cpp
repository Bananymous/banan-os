#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/Device.h>
#include <kernel/FS/Ext2.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/PCI.h>
#include <kernel/Storage/ATAController.h>

namespace Kernel
{

	static VirtualFileSystem* s_instance = nullptr;

	BAN::ErrorOr<void> VirtualFileSystem::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new VirtualFileSystem();
		if (s_instance == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([] { delete s_instance; s_instance = nullptr; } );

		auto partition_inode = TRY(DeviceManager::get().read_directory_inode("hda1"));
		s_instance->m_root_fs = TRY(Ext2FS::create(*(Partition*)partition_inode.ptr()));
		TRY(s_instance->mount(&DeviceManager::get(), "/dev"));

		guard.disable();

		return {};
	}
	
	VirtualFileSystem& VirtualFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(FileSystem* file_system, BAN::StringView path)
	{
		auto file = TRY(file_from_absolute_path(path));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);
		TRY(m_mount_points.push_back({ file, file_system }));
		return {};
	}

	VirtualFileSystem::MountPoint* VirtualFileSystem::mount_point_for_inode(BAN::RefPtr<Inode> inode)
	{
		for (MountPoint& mount : m_mount_points)
			if (*mount.host.inode == *inode)
				return &mount;
		return nullptr;
	}

	BAN::ErrorOr<VirtualFileSystem::File> VirtualFileSystem::file_from_absolute_path(BAN::StringView path)
	{
		ASSERT(path.front() == '/');

		auto inode = root_inode();
		if (!inode)
			return BAN::Error::from_c_string("No root inode available");

		BAN::String canonical_path;

		const auto path_parts = TRY(path.split('/'));

		for (const auto& path_part : path_parts)
		{
			if (path_part.empty() || path_part == "."sv)
			{
				continue;
			}
			else if (path_part == ".."sv)
			{
				if (auto* mount_point = mount_point_for_inode(inode))
					inode = TRY(mount_point->host.inode->read_directory_inode(".."sv));
				else
					inode = TRY(inode->read_directory_inode(".."sv));

				if (!canonical_path.empty())
				{
					while (canonical_path.back() != '/')
						canonical_path.pop_back();
					canonical_path.pop_back();
				}
			}
			else
			{
				inode = TRY(inode->read_directory_inode(path_part));
				TRY(canonical_path.push_back('/'));
				TRY(canonical_path.append(path_part));
			}

			if (auto* mount_point = mount_point_for_inode(inode))
				inode = mount_point->target->root_inode();
		}

		if (canonical_path.empty())
			TRY(canonical_path.push_back('/'));

		File file;
		file.inode = inode;
		file.canonical_path = BAN::move(canonical_path);

		if (file.canonical_path.empty())
			TRY(file.canonical_path.push_back('/'));

		return file;
	}

}