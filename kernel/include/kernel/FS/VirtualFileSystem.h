#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/Lock/Mutex.h>

namespace Kernel
{

	class VirtualFileSystem final : public FileSystem
	{
	public:
		virtual unsigned long bsize()   const override { return 0; }
		virtual unsigned long frsize()  const override { return 0; }
		virtual fsblkcnt_t    blocks()  const override { return 0; }
		virtual fsblkcnt_t    bfree()   const override { return 0; }
		virtual fsblkcnt_t    bavail()  const override { return 0; }
		virtual fsfilcnt_t    files()   const override { return 0; }
		virtual fsfilcnt_t    ffree()   const override { return 0; }
		virtual fsfilcnt_t    favail()  const override { return 0; }
		virtual unsigned long fsid()    const override { return 0; }
		virtual unsigned long flag()    const override { return 0; }
		virtual unsigned long namemax() const override { return 0; }


		static void initialize(BAN::StringView);
		static VirtualFileSystem& get();

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_fs->root_inode(); }

		// FIXME:
		virtual dev_t dev() const override { return 0; }

		BAN::ErrorOr<void> mount(const Credentials&, BAN::StringView, BAN::StringView);
		BAN::ErrorOr<void> mount(const Credentials&, BAN::RefPtr<FileSystem>, BAN::StringView);

		struct File
		{
			File() = default;
			explicit File(BAN::RefPtr<Inode> inode)
				: inode(BAN::move(inode))
			{ }
			explicit File(BAN::RefPtr<Inode> inode, BAN::String&& canonical_path)
				: inode(BAN::move(inode))
				, canonical_path(BAN::move(canonical_path))
			{ }

			File(const File&) = delete;
			File(File&& other)
				: inode(BAN::move(other.inode))
				, canonical_path(BAN::move(other.canonical_path))
			{ }

			File& operator=(const File&) = delete;
			File& operator=(File&& other)
			{
				inode = BAN::move(other.inode);
				canonical_path = BAN::move(other.canonical_path);
				return *this;
			}

			BAN::ErrorOr<File> clone() const
			{
				File result;
				result.inode = inode;
				TRY(result.canonical_path.append(canonical_path));
				return BAN::move(result);
			}

			BAN::RefPtr<Inode> inode;
			BAN::String canonical_path;
		};

		File root_file()
		{
			return File(root_inode(), "/"_sv);
		}

		BAN::ErrorOr<File> file_from_relative_path(const File& parent, const Credentials&, BAN::StringView, int);
		BAN::ErrorOr<File> file_from_absolute_path(const Credentials& credentials, BAN::StringView path, int flags)
		{
			return file_from_relative_path(root_file(), credentials, path, flags);
		}

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
