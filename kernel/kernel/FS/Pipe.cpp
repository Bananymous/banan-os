#include <kernel/FS/Pipe.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

#include <fcntl.h>
#include <sys/epoll.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Inode>> Pipe::create(const Credentials& credentials)
	{
		Pipe* pipe = new Pipe(credentials);
		if (pipe == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Inode>::adopt(pipe);
	}

	Pipe::Pipe(const Credentials& credentials)
		: m_uid(credentials.euid())
		, m_gid(credentials.egid())
	{
		timespec current_time = SystemTimer::get().real_time();
		m_atime = current_time;
		m_mtime = current_time;
		m_ctime = current_time;
	}

	void Pipe::on_clone(int status_flags)
	{
		if (status_flags & O_WRONLY)
		{
			[[maybe_unused]] auto old_writing_count = m_writing_count.fetch_add(1);
			ASSERT(old_writing_count > 0);
		}

		if (status_flags & O_RDONLY)
		{
			[[maybe_unused]] auto old_reading_count = m_reading_count.fetch_add(1);
			ASSERT(old_reading_count > 0);
		}
	}

	void Pipe::on_close(int status_flags)
	{
		LockGuard _(m_mutex);

		if (status_flags & O_WRONLY)
		{
			auto old_writing_count = m_writing_count.fetch_sub(1);
			ASSERT(old_writing_count > 0);
			if (old_writing_count != 1)
				return;
			epoll_notify(EPOLLHUP);
		}

		if (status_flags & O_RDONLY)
		{
			auto old_reading_count = m_reading_count.fetch_sub(1);
			ASSERT(old_reading_count > 0);
			if (old_reading_count != 1)
				return;
			epoll_notify(EPOLLERR);
		}

		m_thread_blocker.unblock();
	}

	BAN::ErrorOr<size_t> Pipe::read_impl(off_t, BAN::ByteSpan buffer)
	{
		while (m_buffer_size == 0)
		{
			if (m_writing_count == 0)
				return 0;
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		const size_t to_copy = BAN::Math::min<size_t>(buffer.size(), m_buffer_size);

		if (m_buffer_tail + to_copy <= m_buffer.size())
			memcpy(buffer.data(), m_buffer.data() + m_buffer_tail, to_copy);
		else
		{
			const size_t before_wrap = m_buffer.size() - m_buffer_tail;
			const size_t after_wrap = to_copy - before_wrap;
			memcpy(buffer.data(), m_buffer.data() + m_buffer_tail, before_wrap);
			memcpy(buffer.data() + before_wrap, m_buffer.data(), after_wrap);
		}

		m_buffer_tail = (m_buffer_tail + to_copy) % m_buffer.size();
		m_buffer_size -= to_copy;

		m_atime = SystemTimer::get().real_time();

		epoll_notify(EPOLLOUT);

		m_thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> Pipe::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		while (m_buffer_size >= m_buffer.size())
		{
			if (m_reading_count == 0)
			{
				Thread::current().add_signal(SIGPIPE, {});
				return BAN::Error::from_errno(EPIPE);
			}
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		const size_t to_copy = BAN::Math::min(buffer.size(), m_buffer.size() - m_buffer_size);
		const size_t buffer_head = (m_buffer_tail + m_buffer_size) % m_buffer.size();

		if (buffer_head + to_copy <= m_buffer.size())
			memcpy(m_buffer.data() + buffer_head, buffer.data(), to_copy);
		else
		{
			const size_t before_wrap = m_buffer.size() - buffer_head;
			const size_t after_wrap = to_copy - before_wrap;
			memcpy(m_buffer.data() + buffer_head, buffer.data(), before_wrap);
			memcpy(m_buffer.data(), buffer.data() + before_wrap, after_wrap);
		}

		m_buffer_size += to_copy;

		timespec current_time = SystemTimer::get().real_time();
		m_mtime = current_time;
		m_ctime = current_time;

		epoll_notify(EPOLLIN);

		m_thread_blocker.unblock();

		return to_copy;
	}

}
