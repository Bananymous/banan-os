#pragma once

#include <BAN/Array.h>
#include <BAN/CircularQueue.h>
#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <kernel/FS/Inode.h>

#include <limits.h>
#include <sys/epoll.h>

namespace Kernel
{

	class Epoll final : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Epoll>> create();
		~Epoll();

		BAN::ErrorOr<void> ctl(int op, int fd, BAN::RefPtr<Inode> inode, epoll_event event);
		BAN::ErrorOr<size_t> wait(BAN::Span<epoll_event> events, uint64_t waketime_ns);

		void notify(BAN::RefPtr<Inode> inode, uint32_t event);

	private:
		Epoll() = default;

	public:
		ino_t ino() const override         { return 0; }
		Mode mode() const override         { return { Mode::IRUSR | Mode::IWUSR }; }
		nlink_t nlink() const override     { return 0; }
		uid_t uid() const override         { return 0; }
		gid_t gid() const override         { return 0; }
		off_t size() const override        { return 0; }
		timespec atime() const override    { return {}; }
		timespec mtime() const override    { return {}; }
		timespec ctime() const override    { return {}; }
		blksize_t blksize() const override { return PAGE_SIZE; }
		blkcnt_t blocks() const override   { return 0; }
		dev_t dev() const override         { return 0; }
		dev_t rdev() const override        { return 0; }

		bool is_epoll() const override { return true; }

		const FileSystem* filesystem() const override { return nullptr; }

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

		BAN::ErrorOr<void> fsync_impl() override { return {}; }

	private:
		struct InodeRefPtrHash
		{
			BAN::hash_t operator()(const BAN::RefPtr<Inode>& inode)
			{
				return BAN::hash<const Inode*>()(inode.ptr());
			}
		};

		struct ListenEventList
		{
			BAN::Array<epoll_event, OPEN_MAX> events;
			uint32_t bitmap[(OPEN_MAX + 31) / 32] {};

			bool has_fd(int fd) const
			{
				if (fd < 0 || static_cast<size_t>(fd) >= events.size())
					return false;
				return bitmap[fd / 32] & (1u << (fd % 32));
			}

			bool empty() const
			{
				for (auto val : bitmap)
					if (val != 0)
						return false;
				return true;
			}

			void add_fd(int fd, epoll_event event)
			{
				ASSERT(!has_fd(fd));
				bitmap[fd / 32] |= (1u << (fd % 32));
				events[fd] = event;
			}

			void remove_fd(int fd)
			{
				ASSERT(has_fd(fd));
				bitmap[fd / 32] &= ~(1u << (fd % 32));
				events[fd] = {};
			}
		};

	private:
		ThreadBlocker m_thread_blocker;
		SpinLock m_ready_lock;
		BAN::HashMap<BAN::RefPtr<Inode>, uint32_t,        InodeRefPtrHash> m_ready_events;
		BAN::HashMap<BAN::RefPtr<Inode>, uint32_t,        InodeRefPtrHash> m_processing_events;
		BAN::HashMap<BAN::RefPtr<Inode>, ListenEventList, InodeRefPtrHash> m_listening_events;
	};

}
