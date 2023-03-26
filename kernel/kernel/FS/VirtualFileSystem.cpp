#include <BAN/StringView.h>
#include <BAN/Vector.h>
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
					if (partition.name() == "banan-root"sv)
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
		return {};
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount_test()
	{
		auto mount = TRY(root_inode()->read_directory_inode("mnt"sv));
		if (!mount->ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		for (auto* controller : m_storage_controllers)
		{
			for (auto* device : controller->devices())
			{
				for (auto& partition : device->partitions())
				{
					if (partition.name() == "mount-test"sv)
					{
						auto ext2fs = TRY(Ext2FS::create(partition));
						TRY(m_mount_points.push_back({ mount, ext2fs }));
						return {};
					}
				}
			}
		}
		return {};
	}

	BAN::ErrorOr<VirtualFileSystem::File> VirtualFileSystem::file_from_absolute_path(BAN::StringView path)
	{
		ASSERT(path.front() == '/');

		auto inode = root_inode();
		if (!inode)
			return BAN::Error::from_c_string("No root inode available");

		auto path_parts = TRY(path.split('/'));

		for (size_t i = 0; i < path_parts.size();)
		{
			if (path_parts[i] == "."sv)
			{
				path_parts.remove(i);
			}
			else if (path_parts[i] == ".."sv)
			{
				inode = TRY(inode->read_directory_inode(path_parts[i]));
				path_parts.remove(i);
				if (i > 0)
				{
					path_parts.remove(i - 1);
					i--;
				}
			}
			else
			{
				inode = TRY(inode->read_directory_inode(path_parts[i]));	
				i++;
			}
		}

		File file;
		file.inode = inode;

		for (const auto& part : path_parts)
		{
			TRY(file.canonical_path.push_back('/'));
			TRY(file.canonical_path.append(part));
		}

		if (file.canonical_path.empty())
			TRY(file.canonical_path.push_back('/'));

		return file;
	}

}