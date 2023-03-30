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

		if (auto res = s_instance->initialize_impl(); res.is_error())
		{
			delete s_instance;
			s_instance = nullptr;
			return res;
		}

		return {};
	}
	
	VirtualFileSystem& VirtualFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<void> VirtualFileSystem::initialize_impl()
	{
		// Initialize all storage controllers
		for (auto& device : PCI::get().devices())
		{
			if (device.class_code() != 0x01)
				continue;
			
			switch (device.subclass())
			{
				case 0x0:
					dwarnln("unsupported SCSI Bus Controller");
					break;
				case 0x1:
				case 0x5:
					TRY(m_storage_controllers.push_back(TRY(ATAController::create(device))));
					break;
				case 0x2:
					dwarnln("unsupported Floppy Disk Controller");
					break;
				case 0x3:
					dwarnln("unsupported IPI Bus Controller");
					break;
				case 0x4:
					dwarnln("unsupported RAID Controller");
					break;
				case 0x6:
					dwarnln("unsupported Serial ATA Controller");
					break;
				case 0x7:
					dwarnln("unsupported Serial Attached SCSI Controller");
					break;
				case 0x8:
					dwarnln("unsupported Non-Volatile Memory Controller");
					break;
				case 0x80:
					dwarnln("unsupported Unknown Storage Controller");
					break;
			}
		}

		// Initialize partitions on all devices on found controllers
		for (auto controller : m_storage_controllers)
		{
			for (auto device : controller->devices())
			{
				if (device->total_size() == 0)
					continue;

				auto result = device->initialize_partitions();
				if (result.is_error())
				{
					dwarnln("{}", result.error());
					continue;
				}
				
				for (auto& partition : device->partitions())
				{
					if (partition.label() == "banan-root"sv)
					{
						if (root_inode())
							dwarnln("multiple root partitions found");
						else
						{
							auto ext2fs_or_error = Ext2FS::create(partition);
							if (ext2fs_or_error.is_error())
								dwarnln("{}", ext2fs_or_error.error());
							else
								// FIXME: We leave a dangling pointer to ext2fs. This might be okay since
								//        root fs sould probably be always mounted
								m_root_inode = ext2fs_or_error.value()->root_inode();
						}
					}
				}
			}
		}

		if (!root_inode())
			derrorln("Could not locate root partition");

		TRY(mount(&DeviceManager::get(), "/dev"));

		return {};
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(FileSystem* file_system, BAN::StringView path)
	{
		auto file = TRY(file_from_absolute_path(path));
		if (!file.inode->ifdir())
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