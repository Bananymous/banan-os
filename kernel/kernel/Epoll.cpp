#include <kernel/Epoll.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Epoll>> Epoll::create()
	{
		auto* epoll_ptr = new Epoll();
		if (epoll_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Epoll>::adopt(epoll_ptr);
	}

	Epoll::~Epoll()
	{
		for (auto [inode, _] : m_listening_events)
			inode->del_epoll(this);
	}

	BAN::ErrorOr<void> Epoll::ctl(int op, int fd, BAN::RefPtr<Inode> inode, epoll_event event)
	{
		LockGuard _(m_mutex);

		auto it = m_listening_events.find(inode);

		switch (op)
		{
			case EPOLL_CTL_ADD:
			{
				bool contains_inode = (it != m_listening_events.end());
				if (!contains_inode)
					it = TRY(m_listening_events.emplace(inode));
				if (it->value.has_fd(fd))
					return BAN::Error::from_errno(EEXIST);

				{
					SpinLockGuard _(m_ready_lock);
					TRY(m_ready_events.reserve(m_listening_events.size()));
				}
				TRY(m_processing_events.reserve(m_listening_events.size()));

				if (!contains_inode)
					TRY(inode->add_epoll(this));
				it->value.add_fd(fd, event);

				SpinLockGuard _(m_ready_lock);
				auto ready_it = m_ready_events.find(inode);
				if (ready_it == m_ready_events.end())
					ready_it = MUST(m_ready_events.insert(inode, 0));
				ready_it->value |= event.events;
				m_thread_blocker.unblock();

				return {};
			}
			case EPOLL_CTL_MOD:
			{
				if (it == m_listening_events.end())
					return BAN::Error::from_errno(ENOENT);
				if (!it->value.has_fd(fd))
					return BAN::Error::from_errno(ENOENT);

				it->value.events[fd] = event;

				SpinLockGuard _(m_ready_lock);
				auto ready_it = m_ready_events.find(inode);
				if (ready_it == m_ready_events.end())
					ready_it = MUST(m_ready_events.insert(inode, 0));
				ready_it->value |= event.events;
				m_thread_blocker.unblock();

				return {};
			}
			case EPOLL_CTL_DEL:
			{
				if (it == m_listening_events.end())
					return BAN::Error::from_errno(ENOENT);
				if (!it->value.has_fd(fd))
					return BAN::Error::from_errno(ENOENT);
				it->value.remove_fd(fd);
				if (it->value.empty())
				{
					inode->del_epoll(this);
					m_listening_events.remove(it);
					m_processing_events.remove(inode);
					SpinLockGuard _(m_ready_lock);
					m_ready_events.remove(inode);
				}
				return {};
			}
		}

		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<size_t> Epoll::wait(BAN::Span<epoll_event> event_span, uint64_t waketime_ns)
	{
		if (event_span.empty())
			return BAN::Error::from_errno(EINVAL);

		size_t event_count = 0;

		for (;;)
		{
			{
				LockGuard _(m_mutex);

				{
					SpinLockGuard _(m_ready_lock);

					while (!m_ready_events.empty())
					{
						auto [inode, events] = *m_ready_events.begin();
						m_ready_events.remove(m_ready_events.begin());

						ASSERT(events);

						if (auto it = m_processing_events.find(inode); it != m_processing_events.end())
							it->value |= events;
						else
							MUST(m_processing_events.insert(inode, events));
					}
				}

				for (auto it = m_processing_events.begin(); it != m_processing_events.end() && event_count < event_span.size();)
				{
					auto& [inode, events] = *it;

#define REMOVE_IT_AND_CONTINUE() \
					({ \
						m_processing_events.remove(it); \
						it = m_processing_events.begin(); \
						continue; \
					})

					auto listen_it = m_listening_events.find(inode);
					if (listen_it == m_listening_events.end())
						REMOVE_IT_AND_CONTINUE();
					auto& listen = listen_it->value;

					{
						uint32_t listen_mask = EPOLLHUP | EPOLLERR;
						for (size_t fd = 0; fd < listen.events.size(); fd++)
							if (listen.has_fd(fd))
								listen_mask |= listen.events[fd].events;
						events &= listen_mask;
					}

					if (events == 0)
						REMOVE_IT_AND_CONTINUE();

					{
						LockGuard inode_locker(inode->m_mutex);

#define CHECK_EVENT_BIT(mask, func) \
						if ((events & mask) && !inode->func()) \
							events &= ~mask;
						CHECK_EVENT_BIT(EPOLLIN, can_read);
						CHECK_EVENT_BIT(EPOLLOUT, can_write);
						CHECK_EVENT_BIT(EPOLLERR, has_error);
						CHECK_EVENT_BIT(EPOLLHUP, has_hungup);
#undef CHECK_EVENT_BIT
					}

					if (events == 0)
						REMOVE_IT_AND_CONTINUE();

#undef REMOVE_IT_AND_CONTINUE

					for (size_t fd = 0; fd < listen.events.size() && event_count < event_span.size(); fd++)
					{
						if (!listen.has_fd(fd))
							continue;
						auto& listen_event = listen.events[fd];

						const auto new_events = (listen_event.events | EPOLLHUP | EPOLLERR) & events;
						if (new_events == 0)
							continue;

						event_span[event_count++] = {
							.events = new_events,
							.data = listen_event.data,
						};

						if (listen_event.events & EPOLLONESHOT)
							listen_event.events = 0;
						// this doesn't work with multiple of the same inode
						if (listen_event.events & EPOLLET)
							events &= ~new_events;
					}

					it++;
				}
			}

			if (event_count > 0)
				break;

			const uint64_t current_ns = SystemTimer::get().ns_since_boot();
			if (current_ns >= waketime_ns)
				break;

			SpinLockGuard guard(m_ready_lock);
			if (!m_ready_events.empty())
				continue;

			SpinLockGuardAsMutex smutex(guard);
			const uint64_t timeout_ns = BAN::Math::min<uint64_t>(100'000'000, waketime_ns - current_ns);
			TRY(Thread::current().block_or_eintr_or_timeout_ns(m_thread_blocker, timeout_ns, false, &smutex));
		}

		return event_count;
	}

	void Epoll::notify(BAN::RefPtr<Inode> inode, uint32_t event)
	{
		ASSERT(event);

		SpinLockGuard _(m_ready_lock);

		if (auto it = m_ready_events.find(inode); it != m_ready_events.end())
			it->value |= event;
		else
			MUST(m_ready_events.insert(inode, event));

		m_thread_blocker.unblock();
	}

}
