#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static void initialize(BAN::StringView);
		static VirtualFileSystem& get();
		virtual ~VirtualFileSystem() {};

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_fs->root_inode(); }

		BAN::ErrorOr<void> mount(const Credentials&, BAN::StringView, BAN::StringView);
		BAN::ErrorOr<void> mount(const Credentials&, FileSystem*, BAN::StringView);

		struct File
		{
			BAN::RefPtr<Inode> inode;
			BAN::String canonical_path;
		};
		BAN::ErrorOr<File> file_from_absolute_path(const Credentials&, BAN::StringView, int);

	private:
		VirtualFileSystem() = default;

		struct MountPoint
		{
			File host;
			FileSystem* target;
		};
		MountPoint* mount_from_host_inode(BAN::RefPtr<Inode>);
		MountPoint* mount_from_root_inode(BAN::RefPtr<Inode>);

	private:
		SpinLock				m_lock;
		FileSystem*				m_root_fs = nullptr;
		BAN::Vector<MountPoint>	m_mount_points;
	};

}