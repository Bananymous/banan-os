#pragma once

#include <BAN/Memory.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <sys/types.h>

namespace Kernel
{

	class Inode : public BAN::RefCounted<Inode>
	{
	public:
		enum Mode : mode_t
		{
			IXOTH = 0x0001,
			IWOTH = 0x0002,
			IROTH = 0x0004,
			IXGRP = 0x0008,
			IWGRP = 0x0010,
			IRGRP = 0x0020,
			IXUSR = 0x0040,
			IWUSR = 0x0080,
			IRUSR = 0x0100,
			ISVTX = 0x0200,
			ISGID = 0x0400,
			ISUID = 0x0800,
			IFIFO = 0x1000,
			IFCHR = 0x2000,
			IFDIR = 0x4000,
			IFREG = 0x8000,
		};

		enum class Type
		{
			Ext2,
		};

	public:
		virtual ~Inode() {}

		bool ifdir() const { return mode() & Mode::IFDIR; }
		bool ifreg() const { return mode() & Mode::IFREG; }

		virtual uid_t uid() const = 0;
		virtual gid_t gid() const = 0;
		virtual size_t size() const = 0;

		virtual mode_t mode() const = 0;

		virtual BAN::StringView name() const = 0;

		BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> directory_inodes();
		BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find(BAN::StringView);

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) = 0;

		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) = 0;

		virtual Type type() const = 0;
		virtual bool operator==(const Inode&) const = 0;

	protected:
		virtual BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> directory_inodes_impl() = 0;
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find_impl(BAN::StringView) = 0;
	};

}