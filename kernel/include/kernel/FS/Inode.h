#pragma once

#include <BAN/Memory.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <sys/types.h>
#include <time.h>

namespace Kernel
{

	class Inode : public BAN::RefCounted<Inode>
	{
	public:
		struct Mode
		{
			enum Mask : mode_t
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
				IFBLK = 0x6000,
				IFREG = 0x8000,
				IFLNK = 0xA000,
				IFSOCK = 0xC000,
				TYPE_MASK = 0xF000,
			};

			bool ifchr()  const { return (mode & Mask::TYPE_MASK) == Mask::IFCHR; }
			bool ifdir()  const { return (mode & Mask::TYPE_MASK) == Mask::IFDIR; }
			bool ifblk()  const { return (mode & Mask::TYPE_MASK) == Mask::IFBLK; }
			bool ifreg()  const { return (mode & Mask::TYPE_MASK) == Mask::IFREG; }
			bool iflnk()  const { return (mode & Mask::TYPE_MASK) == Mask::IFLNK; }
			bool ifsock() const { return (mode & Mask::TYPE_MASK) == Mask::IFSOCK; }
			mode_t mode;
		};

		enum class InodeType
		{
			Device,
			Ext2,
		};

	public:
		virtual ~Inode() {}

		bool operator==(const Inode& other) const { return dev() == other.dev() && rdev() == other.rdev() && ino() == other.ino(); }

		virtual ino_t ino() const = 0;
		virtual Mode mode() const = 0;
		virtual nlink_t nlink() const = 0;
		virtual uid_t uid() const = 0;
		virtual gid_t gid() const = 0;
		virtual off_t size() const = 0;
		virtual timespec atime() const = 0;
		virtual timespec mtime() const = 0;
		virtual timespec ctime() const = 0;
		virtual blksize_t blksize() const = 0;
		virtual blkcnt_t blocks() const = 0;
		virtual dev_t dev() const = 0;
		virtual dev_t rdev() const = 0;

		virtual InodeType inode_type() const = 0;

		virtual BAN::StringView name() const = 0;

		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> read_directory_inode(BAN::StringView) { if (!mode().ifdir()) return BAN::Error::from_errno(ENOTDIR); ASSERT_NOT_REACHED(); }
		virtual BAN::ErrorOr<BAN::Vector<BAN::String>> read_directory_entries(size_t)  { if (!mode().ifdir()) return BAN::Error::from_errno(ENOTDIR); ASSERT_NOT_REACHED(); }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t)        { if ( mode().ifdir()) return BAN::Error::from_errno(EISDIR);  ASSERT_NOT_REACHED(); }
		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) { if (!mode().ifdir()) return BAN::Error::from_errno(ENOTDIR); ASSERT_NOT_REACHED(); }
	};

}