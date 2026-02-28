#include <kernel/FS/Pipe.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

#include <fcntl.h>
#include <sys/epoll.h>

namespace Kernel
{

	static constexpr size_t s_pipe_buffer_size = 0x10000;

	BAN::ErrorOr<BAN::RefPtr<Inode>> Pipe::create(const Credentials& credentials)
	{
		auto* pipe_ptr = new Pipe(credentials);
		if (pipe_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto pipe = BAN::RefPtr<Pipe>::adopt(pipe_ptr);
		pipe->m_buffer = TRY(ByteRingBuffer::create(s_pipe_buffer_size));
		return BAN::RefPtr<Inode>(pipe);
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
		while (m_buffer->empty())
		{
			if (m_writing_count == 0)
				return 0;
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		const size_t to_copy = BAN::Math::min<size_t>(buffer.size(), m_buffer->size());
		memcpy(buffer.data(), m_buffer->get_data().data(), to_copy);
		m_buffer->pop(to_copy);

		m_atime = SystemTimer::get().real_time();

		epoll_notify(EPOLLOUT);

		m_thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> Pipe::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		while (m_buffer->full())
		{
			if (m_reading_count == 0)
			{
				Thread::current().add_signal(SIGPIPE, {});
				return BAN::Error::from_errno(EPIPE);
			}
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		const size_t to_copy = BAN::Math::min(buffer.size(), m_buffer->free());
		m_buffer->push(buffer.slice(0, to_copy));

		timespec current_time = SystemTimer::get().real_time();
		m_mtime = current_time;
		m_ctime = current_time;

		epoll_notify(EPOLLIN);

		m_thread_blocker.unblock();

		return to_copy;
	}

}
