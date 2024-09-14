#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Storage/Partition.h>

#include <fcntl.h>

namespace Kernel
{

	static BAN::RefPtr<VirtualFileSystem> s_instance;

	void VirtualFileSystem::initialize(BAN::StringView root_path)
	{
		ASSERT(!s_instance);
		s_instance = MUST(BAN::RefPtr<VirtualFileSystem>::create());

		BAN::RefPtr<BlockDevice> root_device;
		if (root_path.size() >= 5 && root_path.substring(0, 5) == "UUID="_sv)
		{
			auto uuid = root_path.substring(5);
			if (uuid.size() != 36)
				panic("Invalid UUID specified for root '{}'", uuid);

			BAN::RefPtr<Partition> root_partition;
			DevFileSystem::get().for_each_inode(
				[&root_partition, uuid](BAN::RefPtr<Inode> inode) -> BAN::Iteration
				{
					if (!inode->is_device())
						return BAN::Iteration::Continue;
					if (!static_cast<Device*>(inode.ptr())->is_partition())
						return BAN::Iteration::Continue;
					auto* partition = static_cast<Partition*>(inode.ptr());
					if (partition->uuid() != uuid)
						return BAN::Iteration::Continue;
					root_partition = partition;
					return BAN::Iteration::Break;
				}
			);
			if (!root_partition)
				panic("Could not find partition with UUID '{}'", uuid);
			root_device = root_partition;
		}
		else if (root_path.size() >= 5 && root_path.substring(0, 5) == "/dev/"_sv)
		{
			auto device_name = root_path.substring(5);

			auto device_result = DevFileSystem::get().root_inode()->find_inode(device_name);
			if (device_result.is_error())
				panic("Could not open root device '{}': {}", root_path, device_result.error());

			auto device_inode = device_result.release_value();
			if (!device_inode->mode().ifblk())
				panic("Root inode '{}' is not an block device", root_path);

			root_device = static_cast<BlockDevice*>(device_inode.ptr());
		}
		else
		{
			panic("Unknown root path format '{}' specified", root_path);
		}

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
