#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/Lock/Mutex.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static void initialize(BAN::StringView);
		static VirtualFileSystem& get();

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_fs->root_inode(); }

		// FIXME:
		virtual dev_t dev() const override { return 0; }

		BAN::ErrorOr<void> mount(const Credentials&, BAN::StringView, BAN::StringView);
		BAN::ErrorOr<void> mount(const Credentials&, BAN::RefPtr<FileSystem>, BAN::StringView);

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
			BAN::RefPtr<FileSystem> target;
			File host;
		};
		MountPoint* mount_from_host_inode(BAN::RefPtr<Inode>);
		MountPoint* mount_from_root_inode(BAN::RefPtr<Inode>);

	private:
		Mutex					m_mutex;
		BAN::RefPtr<FileSystem>	m_root_fs;
		BAN::Vector<MountPoint>	m_mount_points;

		friend class BAN::RefPtr<VirtualFileSystem>;
	};

}
