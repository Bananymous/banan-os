#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Storage/Partition.h>
#include <kernel/Timer/Timer.h>

#include <fcntl.h>

namespace Kernel
{

	static BAN::RefPtr<VirtualFileSystem> s_instance;

	static BAN::ErrorOr<BAN::RefPtr<BlockDevice>> find_partition_by_uuid(BAN::StringView uuid)
	{
		ASSERT(uuid.size() == 36);

		BAN::RefPtr<BlockDevice> result;
		DevFileSystem::get().for_each_inode(
			[&result, uuid](BAN::RefPtr<Inode> inode) -> BAN::Iteration
			{
				if (!inode->is_device())
					return BAN::Iteration::Continue;
				if (!static_cast<Device*>(inode.ptr())->is_partition())
					return BAN::Iteration::Continue;
				auto* partition = static_cast<Partition*>(inode.ptr());
				if (partition->uuid() != uuid)
					return BAN::Iteration::Continue;
				result = partition;
				return BAN::Iteration::Break;
			}
		);

		if (!result)
			return BAN::Error::from_errno(ENOENT);
		return result;
	}

	static BAN::ErrorOr<BAN::RefPtr<BlockDevice>> find_block_device_by_name(BAN::StringView name)
	{
		auto device_inode = TRY(DevFileSystem::get().root_inode()->find_inode(name));
		if (!device_inode->mode().ifblk())
			return BAN::Error::from_errno(ENOTBLK);
		return BAN::RefPtr<BlockDevice>(static_cast<BlockDevice*>(device_inode.ptr()));
	}

	static BAN::RefPtr<BlockDevice> find_root_device(BAN::StringView root_path)
	{
		enum class RootType
		{
			PartitionUUID,
			BlockDeviceName,
		};

		BAN::StringView entry;
		RootType type;

		if (root_path.starts_with("PARTUUID="_sv))
		{
			entry = root_path.substring(9);
			if (entry.size() != 36)
				panic("Invalid UUID '{}'", entry);
			type = RootType::PartitionUUID;
		}
		else if (root_path.starts_with("/dev/"_sv))
		{
			entry = root_path.substring(5);
			if (entry.empty() || entry.contains('/'))
				panic("Invalid root path '{}'", root_path);
			type = RootType::BlockDeviceName;
		}
		else
		{
			panic("Unsupported root path format '{}'", root_path);
		}

		constexpr size_t timeout_ms = 10'000;
		constexpr size_t sleep_ms = 500;

		for (size_t i = 0; i < timeout_ms / sleep_ms; i++)
		{
			BAN::ErrorOr<BAN::RefPtr<BlockDevice>> ret = BAN::Error::from_errno(EINVAL);

			switch (type)
			{
				case RootType::PartitionUUID:
					ret = find_partition_by_uuid(entry);
					break;
				case RootType::BlockDeviceName:
					ret = find_block_device_by_name(entry);
					break;
			}

			if (!ret.is_error())
				return ret.release_value();

			if (ret.error().get_error_code() != ENOENT)
				panic("could not open root device '{}': {}", root_path, ret.error());

			SystemTimer::get().sleep_ms(sleep_ms);
		}

		panic("could not find root device '{}' after {} ms", root_path, timeout_ms);
	}

	void VirtualFileSystem::initialize(BAN::StringView root_path)
	{
		ASSERT(!s_instance);
		s_instance = MUST(BAN::RefPtr<VirtualFileSystem>::create());

		auto root_device = find_root_device(root_path);
		ASSERT(root_device);

		auto filesystem_result = FileSystem::from_block_device(root_device);
		if (filesystem_result.is_error())
			panic("Could not create filesystem from '{}': {}", root_path, filesystem_result.error());
		s_instance->m_root_fs = filesystem_result.release_value();

		Credentials root_creds { 0, 0, 0, 0 };
		MUST(s_instance->mount(root_creds, &DevFileSystem::get(), "/dev"_sv));

		MUST(s_instance->mount(root_creds, &ProcFileSystem::get(), "/proc"_sv));

		auto tmpfs = MUST(TmpFileSystem::create(1024, 0777, 0, 0));
		MUST(s_instance->mount(root_creds, tmpfs, "/tmp"_sv));
	}

	VirtualFileSystem& VirtualFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(const Credentials& credentials, BAN::StringView block_device_path, BAN::StringView target)
	{
		auto block_device_file = TRY(file_from_absolute_path(credentials, block_device_path, true));
		if (!block_device_file.inode->is_device())
			return BAN::Error::from_errno(ENOTBLK);

		auto* device = static_cast<Device*>(block_device_file.inode.ptr());
		if (!device->mode().ifblk())
			return BAN::Error::from_errno(ENOTBLK);

		auto file_system = TRY(FileSystem::from_block_device(static_cast<BlockDevice*>(device)));
		return mount(credentials, file_system, target);
	}

	BAN::ErrorOr<void> VirtualFileSystem::mount(const Credentials& credentials, BAN::RefPtr<FileSystem> file_system, BAN::StringView path)
	{
		auto file = TRY(file_from_absolute_path(credentials, path, true));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_mutex);
		TRY(m_mount_points.emplace_back(file_system, BAN::move(file)));
		return {};
	}

	VirtualFileSystem::MountPoint* VirtualFileSystem::mount_from_host_inode(BAN::RefPtr<Inode> inode)
	{
		LockGuard _(m_mutex);
		for (MountPoint& mount : m_mount_points)
			if (*mount.host.inode == *inode)
				return &mount;
		return nullptr;
	}

	VirtualFileSystem::MountPoint* VirtualFileSystem::mount_from_root_inode(BAN::RefPtr<Inode> inode)
	{
		LockGuard _(m_mutex);
		for (MountPoint& mount : m_mount_points)
			if (*mount.target->root_inode() == *inode)
				return &mount;
		return nullptr;
	}

	BAN::ErrorOr<VirtualFileSystem::File> VirtualFileSystem::file_from_relative_path(const File& parent, const Credentials& credentials, BAN::StringView path, int flags)
	{
		LockGuard _(m_mutex);

		auto inode = parent.inode;
		ASSERT(inode);

		BAN::String canonical_path;
		TRY(canonical_path.append(parent.canonical_path));
		if (!canonical_path.empty() && canonical_path.back() == '/')
			canonical_path.pop_back();
		ASSERT(canonical_path.empty() || canonical_path.back() != '/');

		BAN::Vector<BAN::String> path_parts;

		const auto append_string_view_in_reverse =
			[&path_parts](BAN::StringView path) -> BAN::ErrorOr<void>
			{
				auto split_path = TRY(path.split('/'));
				split_path.reverse();
				for (auto part : split_path)
				{
					TRY(path_parts.emplace_back());
					TRY(path_parts.back().append(part));
				}
				return {};
			};
		TRY(append_string_view_in_reverse(path));

		size_t link_depth = 0;

		while (!path_parts.empty())
		{
			BAN::String path_part = BAN::move(path_parts.back());
			path_parts.pop_back();

			if (path_part.empty() || path_part == "."_sv)
				continue;

			auto orig = inode;

			// resolve file name
			{
				auto parent_inode = inode;
				if (path_part == ".."_sv)
					if (auto* mount_point = mount_from_root_inode(inode))
						parent_inode = mount_point->host.inode;
				if (!parent_inode->can_access(credentials, O_SEARCH))
					return BAN::Error::from_errno(EACCES);
				inode = TRY(parent_inode->find_inode(path_part));

				if (path_part == ".."_sv)
				{
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
					if (auto* mount_point = mount_from_host_inode(inode))
						inode = mount_point->target->root_inode();
					TRY(canonical_path.push_back('/'));
					TRY(canonical_path.append(path_part));
				}
			}

			if (!inode->mode().iflnk())
				continue;
			if ((flags & O_NOFOLLOW) && path_parts.empty())
				continue;

			// resolve symbolic links
			{
				auto link_target = TRY(inode->link_target());
				if (link_target.empty())
					return BAN::Error::from_errno(ENOENT);

				if (link_target.front() == '/')
				{
					inode = root_inode();
					canonical_path.clear();
				}
				else
				{
					inode = orig;

					while (canonical_path.back() != '/')
						canonical_path.pop_back();
					canonical_path.pop_back();
				}

				TRY(append_string_view_in_reverse(link_target.sv()));

				link_depth++;
				if (link_depth > 100)
					return BAN::Error::from_errno(ELOOP);
			}
		}

		if (!inode->can_access(credentials, flags))
			return BAN::Error::from_errno(EACCES);

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
