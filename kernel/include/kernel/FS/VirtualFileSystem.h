#pragma once

#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static BAN::ErrorOr<void> initialize(BAN::StringView);
		static VirtualFileSystem& get();
		virtual ~VirtualFileSystem() {};

		virtual BAN::RefPtr<Inode> root_inode() override  { return m_root_fs->root_inode(); }

		BAN::ErrorOr<void> mount(BAN::StringView, BAN::StringView);

		struct File
		{
			BAN::RefPtr<Inode> inode;
			BAN::String canonical_path;
		};
		BAN::ErrorOr<File> file_from_absolute_path(BAN::StringView);

	private:
		VirtualFileSystem() = default;
		BAN::ErrorOr<void> mount(FileSystem*, BAN::StringView);

		struct MountPoint
		{
			File host;
			FileSystem* target;
		};
		MountPoint* mount_point_for_inode(BAN::RefPtr<Inode>);

	private:
		FileSystem*						m_root_fs = nullptr;
		BAN::Vector<MountPoint>			m_mount_points;
		BAN::Vector<StorageController*>	m_storage_controllers;
	};

}