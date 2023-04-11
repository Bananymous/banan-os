#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/DeviceManager.h>
#include <kernel/FS/Ext2.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/LockGuard.h>

namespace Kernel
{

	static VirtualFileSystem* s_instance = nullptr;

	BAN::ErrorOr<void> VirtualFileSystem::initialize(BAN::StringView root)
	{
		ASSERT(s_instance == nullptr);
		s_instance = new VirtualFileSystem();
		if (s_instance == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard guard([] { delete s_instance; s_instance = nullptr; } );

		ASSERT(root.size() >= 5 && root.substring(0, 5) == "/dev/"sv);;
		root = root.substring(5);

		auto partition_inode = TRY(DeviceManager::get().read_directory_inode(root));
		s_instance->m_root_fs = TRY(Ext2FS::create(*(Partition*)partition_inode.ptr()));

		DeviceManager::get().set_blksize(s_instance->m_root_fs->root_inode()->blksize());
		TRY(s_instance->mount(&DeviceManager::get(), "/dev"));

		guard.disable();

		return {};
	}
	
	VirtualFileSystem& VirtualFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(BAN::StringView partition, BAN::StringView target)
	{
		auto partition_file = TRY(file_from_absolute_path(partition));
		if (partition_file.inode->inode_type() != Inode::InodeType::Device)
			return BAN::Error::from_errno(ENOTBLK);

		Device* device = (Device*)partition_file.inode.ptr();
		if (device->device_type() != Device::DeviceType::BlockDevice)
			return BAN::Error::from_errno(ENOTBLK);

		auto* file_system = TRY(Ext2FS::create(*(Partition*)device));
		return mount(file_system, target);
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(FileSystem* file_system, BAN::StringView path)
	{
		auto file = TRY(file_from_absolute_path(path));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_lock);
		TRY(m_mount_points.push_back({ file, file_system }));

		return {};
	}

	VirtualFileSystem::MountPoint* VirtualFileSystem::mount_from_host_inode(BAN::RefPtr<Inode> inode)
	{
		ASSERT(m_lock.is_locked());
		for (MountPoint& mount : m_mount_points)
			if (*mount.host.inode == *inode)
				return &mount;
		return nullptr;
	}

	VirtualFileSystem::MountPoint* VirtualFileSystem::mount_from_root_inode(BAN::RefPtr<Inode> inode)
	{
		ASSERT(m_lock.is_locked());
		for (MountPoint& mount : m_mount_points)
			if (*mount.target->root_inode() == *inode)
				return &mount;
		return nullptr;
	}

	BAN::ErrorOr<VirtualFileSystem::File> VirtualFileSystem::file_from_absolute_path(BAN::StringView path)
	{
		LockGuard _(m_lock);

		ASSERT(path.front() == '/');

		auto inode = root_inode();
		ASSERT(inode);

		BAN::String canonical_path;

		const auto path_parts = TRY(path.split('/'));

		for (auto path_part : path_parts)
		{
			if (path_part.empty() || path_part == "."sv)
			{
				continue;
			}
			else if (path_part == ".."sv)
			{
				if (auto* mount_point = mount_from_root_inode(inode))
					inode = TRY(mount_point->host.inode->read_directory_inode(".."sv));
				else
					inode = TRY(inode->read_directory_inode(".."sv));

				if (!canonical_path.empty())
				{
					ASSERT(canonical_path.front() == '/');
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

				if (auto* mount_point = mount_from_host_inode(inode))
					inode = mount_point->target->root_inode();
			}
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