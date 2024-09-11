#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/RefPtr.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <BAN/WeakPtr.h>

#include <kernel/Credentials.h>
#include <kernel/Debug.h>
#include <kernel/Lock/Mutex.h>

#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

namespace Kernel
{

	class FileBackedRegion;
	class SharedFileData;

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
			bool ififo()  const { return (mode & Mask::TYPE_MASK) == Mask::IFIFO; }
			bool iflnk()  const { return (mode & Mask::TYPE_MASK) == Mask::IFLNK; }
			bool ifsock() const { return (mode & Mask::TYPE_MASK) == Mask::IFSOCK; }
			mode_t mode;
		};

	public:
		virtual ~Inode() {}

		bool can_access(const Credentials&, int);

		bool operator==(const Inode& other) const { return dev() == other.dev() && ino() == other.ino(); }

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

		virtual bool is_device() const { return false; }
		virtual bool is_pipe() const { return false; }
		virtual bool is_tty() const { return false; }

		// Directory API
		BAN::ErrorOr<BAN::RefPtr<Inode>> find_inode(BAN::StringView);
		BAN::ErrorOr<size_t> list_next_inodes(off_t, struct dirent* list, size_t list_size);
		BAN::ErrorOr<void> create_file(BAN::StringView, mode_t, uid_t, gid_t);
		BAN::ErrorOr<void> create_directory(BAN::StringView, mode_t, uid_t, gid_t);
		BAN::ErrorOr<void> unlink(BAN::StringView);

		// Link API
		BAN::ErrorOr<BAN::String> link_target();

		// Socket API
		BAN::ErrorOr<long> accept(sockaddr* address, socklen_t* address_len, int flags);
		BAN::ErrorOr<void> bind(const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<void> connect(const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<void> listen(int backlog);
		BAN::ErrorOr<size_t> sendto(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<size_t> recvfrom(BAN::ByteSpan buffer, sockaddr* address, socklen_t* address_len);
		BAN::ErrorOr<void> getsockname(sockaddr* address, socklen_t* address_len);

		// General API
		BAN::ErrorOr<size_t> read(off_t, BAN::ByteSpan buffer);
		BAN::ErrorOr<size_t> write(off_t, BAN::ConstByteSpan buffer);
		BAN::ErrorOr<void> truncate(size_t);
		BAN::ErrorOr<void> chmod(mode_t);
		BAN::ErrorOr<void> chown(uid_t, gid_t);

		// Select/Non blocking API
		bool can_read() const;
		bool can_write() const;
		bool has_error() const;

		BAN::ErrorOr<long> ioctl(int request, void* arg);

	protected:
		// Directory API
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> find_inode_impl(BAN::StringView)				{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> list_next_inodes_impl(off_t, struct dirent*, size_t)		{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> create_file_impl(BAN::StringView, mode_t, uid_t, gid_t)		{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> create_directory_impl(BAN::StringView, mode_t, uid_t, gid_t)	{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> unlink_impl(BAN::StringView)									{ return BAN::Error::from_errno(ENOTSUP); }

		// Link API
		virtual BAN::ErrorOr<BAN::String> link_target_impl()				{ return BAN::Error::from_errno(ENOTSUP); }

		// Socket API
		virtual BAN::ErrorOr<long> accept_impl(sockaddr*, socklen_t*, int)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> listen_impl(int)													{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> bind_impl(const sockaddr*, socklen_t)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> sendto_impl(BAN::ConstByteSpan, const sockaddr*, socklen_t)	{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> recvfrom_impl(BAN::ByteSpan, sockaddr*, socklen_t*)			{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> getsockname_impl(sockaddr*, socklen_t*)							{ return BAN::Error::from_errno(ENOTSUP); }

		// General API
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan)		{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan)	{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t)					{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> chmod_impl(mode_t)						{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> chown_impl(uid_t, gid_t)					{ return BAN::Error::from_errno(ENOTSUP); }

		// Select/Non blocking API
		virtual bool can_read_impl() const = 0;
		virtual bool can_write_impl() const = 0;
		virtual bool has_error_impl() const = 0;

		virtual BAN::ErrorOr<long> ioctl_impl(int, void*) { return BAN::Error::from_errno(ENOTSUP); }

	protected:
		mutable PriorityMutex m_mutex;

	private:
		BAN::WeakPtr<SharedFileData> m_shared_region;
		friend class FileBackedRegion;
		friend class SharedFileData;
	};

}
