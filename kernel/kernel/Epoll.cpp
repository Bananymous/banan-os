#include <kernel/Epoll.h>
#include <kernel/Lock/LockGuard.h>
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

	BAN::ErrorOr<void> Epoll::ctl(int op, BAN::RefPtr<Inode> inode, epoll_event event)
	{
		LockGuard _(m_mutex);

		auto it = m_listening_events.find(inode);

		switch (op)
		{
			case EPOLL_CTL_ADD:
				if (it != m_listening_events.end())
					return BAN::Error::from_errno(EEXIST);
				TRY(m_listening_events.reserve(m_listening_events.size() + 1));
				TRY(m_ready_events.reserve(m_listening_events.size() + 1));
				TRY(inode->add_epoll(this));
				MUST(m_listening_events.insert(inode, event));
				MUST(m_ready_events.insert(inode, event.events));
				return {};
			case EPOLL_CTL_MOD:
				if (it == m_listening_events.end())
					return BAN::Error::from_errno(ENOENT);
				MUST(m_ready_events.emplace_or_assign(inode, event.events));
				it->value = event;
				return {};
			case EPOLL_CTL_DEL:
				if (it == m_listening_events.end())
					return BAN::Error::from_errno(ENOENT);
				m_listening_events.remove(it);
				m_ready_events.remove(inode);
				inode->del_epoll(this);
				return {};
		}

		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<size_t> Epoll::wait(BAN::Span<epoll_event> event_span, uint64_t waketime_ns)
	{
		size_t count = 0;

		for (;;)
		{
			{
				LockGuard _(m_mutex);
				for (auto it = m_ready_events.begin(); it != m_ready_events.end() && count < event_span.size();)
				{
					auto& [inode, events] = *it;

					auto& listen = m_listening_events[inode];
					const uint32_t listen_mask = (listen.events & (EPOLLIN | EPOLLOUT)) | EPOLLERR | EPOLLHUP;

					events &= listen_mask;
#define CHECK_EVENT_BIT(mask, func) \
					if ((events & mask) && !inode->func()) \
						events &= ~mask;
					CHECK_EVENT_BIT(EPOLLIN, can_read);
					CHECK_EVENT_BIT(EPOLLOUT, can_write);
					CHECK_EVENT_BIT(EPOLLERR, has_error);
					CHECK_EVENT_BIT(EPOLLHUP, has_hangup);
#undef CHECK_EVENT_BIT

					if (events == 0)
					{
						m_ready_events.remove(it);
						it = m_ready_events.begin();
						continue;
					}

					event_span[count++] = {
						.events = events,
						.data = listen.data,
					};

					if (listen.events & EPOLLONESHOT)
						listen.events = 0;

					if (listen.events & EPOLLET)
						events &= ~listen_mask;

					it++;
				}
			}

			if (count)
				break;

			const uint64_t current_ns = SystemTimer::get().ns_since_boot();
			if (current_ns >= waketime_ns)
				break;
			const uint64_t timeout_ns = BAN::Math::min<uint64_t>(100'000'000, waketime_ns - current_ns);
			TRY(Thread::current().block_or_eintr_or_timeout_ns(m_thread_blocker, timeout_ns, false));
		}

		return count;
	}

	void Epoll::notify(BAN::RefPtr<Inode> inode, uint32_t event)
	{
		LockGuard _(m_mutex);

		auto listen_it = m_listening_events.find(inode);
		if (listen_it == m_listening_events.end())
			return;

		event &= (listen_it->value.events & (EPOLLIN | EPOLLOUT)) | EPOLLERR | EPOLLHUP;
		if (event == 0)
			return;

		if (auto ready_it = m_ready_events.find(inode); ready_it != m_ready_events.end())
			ready_it->value |= event;
		else
			MUST(m_ready_events.insert(inode, event));

		m_thread_blocker.unblock();
	}

}
