#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/LinkedList.h>
#include <BAN/RefPtr.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <BAN/WeakPtr.h>

#include <kernel/Credentials.h>
#include <kernel/Debug.h>

#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

namespace Kernel
{

	class FileBackedRegion;
	class FileSystem;
	struct SharedFileData;

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
		enum InodeKind : uint8_t {
			DEVICE = 0x1,
			EPOLL  = 0x2,
			PIPE   = 0x4,
			TTY    = 0x8,
		};
	public:
		virtual ~Inode() {}

		bool can_access(const Credentials&, int) const;

		bool operator==(const Inode& other) const { return dev() == other.dev() && ino() == other.ino(); }

		ino_t ino() const { return m_ino; }
		Mode mode() const { return Mode(m_mode); }
		nlink_t nlink() const { return m_nlink; }
		uid_t uid() const { return m_uid; }
		gid_t gid() const { return m_gid; }
		off_t size() const { return m_size; }
		timespec atime() const { return m_atime; }
		timespec mtime() const { return m_mtime; }
		timespec ctime() const { return m_ctime; }
		blksize_t blksize() const { return m_blksize; }
		blkcnt_t blocks() const { return m_blocks; }
		dev_t dev() const { return m_dev; }
		dev_t rdev() const { return m_rdev; }

		bool is_device() const { return m_kind & InodeKind::DEVICE; }
		bool is_epoll()  const { return m_kind & InodeKind::EPOLL;  }
		bool is_pipe()   const { return m_kind & InodeKind::PIPE;   }
		bool is_tty()    const { return m_kind & InodeKind::TTY;    }

		virtual const FileSystem* filesystem() const = 0;

		// Directory API
		BAN::ErrorOr<BAN::RefPtr<Inode>> find_inode(BAN::StringView);
		BAN::ErrorOr<size_t> list_next_inodes(off_t, struct dirent* list, size_t list_size);
		BAN::ErrorOr<void> create_file(BAN::StringView, mode_t, uid_t, gid_t);
		BAN::ErrorOr<void> create_directory(BAN::StringView, mode_t, uid_t, gid_t);
		BAN::ErrorOr<void> link_inode(BAN::StringView, BAN::RefPtr<Inode>);
		BAN::ErrorOr<void> rename_inode(BAN::RefPtr<Inode>, BAN::StringView, BAN::StringView);
		BAN::ErrorOr<void> unlink(BAN::StringView);

		// Link API
		BAN::ErrorOr<BAN::String> link_target();
		BAN::ErrorOr<void> set_link_target(BAN::StringView);

		// Socket API
		BAN::ErrorOr<long> accept(sockaddr* address, socklen_t* address_len, int flags);
		BAN::ErrorOr<void> bind(const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<void> connect(const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<void> listen(int backlog);
		BAN::ErrorOr<size_t> sendmsg(const msghdr& message, int flags);
		BAN::ErrorOr<size_t> recvmsg(msghdr& message, int flags);
		BAN::ErrorOr<void> getsockname(sockaddr* address, socklen_t* address_len);
		BAN::ErrorOr<void> getpeername(sockaddr* address, socklen_t* address_len);
		BAN::ErrorOr<void> getsockopt(int level, int option, void* value, socklen_t* value_len);
		BAN::ErrorOr<void> setsockopt(int level, int option, const void* value, socklen_t value_len);

		// General API
		BAN::ErrorOr<size_t> read(off_t, BAN::ByteSpan buffer);
		BAN::ErrorOr<size_t> write(off_t, BAN::ConstByteSpan buffer);
		BAN::ErrorOr<void> truncate(size_t);
		BAN::ErrorOr<void> chmod(mode_t);
		BAN::ErrorOr<void> chown(uid_t, gid_t);
		BAN::ErrorOr<void> utimens(const timespec[2]);
		BAN::ErrorOr<void> fsync();

		// Select/Non blocking API
		bool can_read() const;
		bool can_write() const;
		bool has_error() const;
		bool has_hungup() const;

		BAN::ErrorOr<long> ioctl(int request, void* arg);

		BAN::ErrorOr<void> add_epoll(class Epoll*);
		void del_epoll(class Epoll*);
		void epoll_notify(uint32_t event);

		virtual void on_close(int status_flags) { (void)status_flags; }
		virtual void on_clone(int status_flags) { (void)status_flags; }

	protected:
		// Directory API
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> find_inode_impl(BAN::StringView)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> list_next_inodes_impl(off_t, struct dirent*, size_t)					{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> create_file_impl(BAN::StringView, mode_t, uid_t, gid_t)					{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> create_directory_impl(BAN::StringView, mode_t, uid_t, gid_t)				{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> link_inode_impl(BAN::StringView, BAN::RefPtr<Inode>)						{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> rename_inode_impl(BAN::RefPtr<Inode>, BAN::StringView, BAN::StringView)	{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> unlink_impl(BAN::StringView)												{ return BAN::Error::from_errno(ENOTSUP); }

		// Link API
		virtual BAN::ErrorOr<BAN::String> link_target_impl()				{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> set_link_target_impl(BAN::StringView)	{ return BAN::Error::from_errno(ENOTSUP); }

		// Socket API
		virtual BAN::ErrorOr<long> accept_impl(sockaddr*, socklen_t*, int)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> listen_impl(int)													{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> bind_impl(const sockaddr*, socklen_t)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> recvmsg_impl(msghdr&, int)										{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> sendmsg_impl(const msghdr&, int)								{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> getsockname_impl(sockaddr*, socklen_t*)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> getpeername_impl(sockaddr*, socklen_t*)							{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> getsockopt_impl(int, int, void*, socklen_t*)						{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> setsockopt_impl(int, int, const void*, socklen_t)				{ return BAN::Error::from_errno(ENOTSUP); }

		// General API
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan)		{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan)	{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t)					{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> chmod_impl(mode_t)						{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> chown_impl(uid_t, gid_t)					{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> utimens_impl(const timespec[2])			{ return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> fsync_impl() = 0;

		// Select/Non blocking API
		virtual bool can_read_impl() const = 0;
		virtual bool can_write_impl() const = 0;
		virtual bool has_error_impl() const = 0;
		virtual bool has_hungup_impl() const = 0;

		virtual BAN::ErrorOr<long> ioctl_impl(int, void*) { return BAN::Error::from_errno(ENOTSUP); }

	protected:
		// TODO: this is supposed to be const I guess?
		// But the thing is I would have to refactor a big chunk of the codebase
		// to add it as a parameter to Inode() soooooo yeah no, not doing that rn.
		uint8_t m_kind = 0;
		BAN::Atomic<ino_t>     m_ino;
		BAN::Atomic<mode_t>    m_mode;
		BAN::Atomic<nlink_t>   m_nlink;
		BAN::Atomic<uid_t>     m_uid;
		BAN::Atomic<gid_t>     m_gid;
		BAN::Atomic<off_t>     m_size;
		// TODO: make these guys atomic :)
		timespec m_atime;
		timespec m_mtime;
		timespec m_ctime;
		BAN::Atomic<blksize_t> m_blksize;
		BAN::Atomic<blkcnt_t>  m_blocks;
		BAN::Atomic<dev_t>     m_dev;
		BAN::Atomic<dev_t>     m_rdev;
	private:
		SpinLock m_shared_region_lock;
		BAN::WeakPtr<SharedFileData> m_shared_region;

		SpinLock m_epoll_lock;
		BAN::LinkedList<class Epoll*> m_epolls;
		friend class Epoll;
		friend class FileBackedRegion;
		friend class OpenFileDescriptorSet;
		friend struct SharedFileData;
		friend class TTY;
	};

}
