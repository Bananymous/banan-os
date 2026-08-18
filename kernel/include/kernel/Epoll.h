#pragma once

#include <BAN/HashMap.h>
#include <kernel/FS/Inode.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/ThreadBlocker.h>

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
		Epoll();

		const FileSystem* filesystem() const override { return nullptr; }

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

		BAN::ErrorOr<void> sync_inode(SyncType) override { return {}; }
		BAN::ErrorOr<void> sync_data() override { return {}; }

	private:
		struct ListenEventList
		{
			ListenEventList() = default;

			ListenEventList(const ListenEventList&) = delete;
			ListenEventList& operator=(const ListenEventList&) = delete;

			ListenEventList(ListenEventList&& other)
				: events(BAN::move(other.events))
			{}
			ListenEventList& operator=(ListenEventList&& other)
			{
				events = BAN::move(other.events);
				return *this;
			}

			BAN::HashMap<int, epoll_event> events;

			bool has_fd(int fd) const
			{
				return events.contains(fd);
			}

			bool empty() const
			{
				return events.empty();
			}

			BAN::ErrorOr<void> add_fd(int fd, epoll_event event)
			{
				TRY(events.insert(fd, event));
				return {};
			}

			void remove_fd(int fd)
			{
				events.remove(fd);
			}
		};

	private:
		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;
		SpinLock m_ready_lock;
		BAN::HashMap<BAN::RefPtr<Inode>, uint32_t> m_ready_events;
		BAN::HashMap<BAN::RefPtr<Inode>, uint32_t> m_processing_events;
		BAN::HashMap<BAN::RefPtr<Inode>, ListenEventList> m_listening_events;
	};

}
