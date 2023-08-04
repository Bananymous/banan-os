#include <kernel/FS/Pipe.h>
#include <kernel/LockGuard.h>
#include <kernel/Timer/Timer.h>

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
		uint64_t current_time = SystemTimer::get().get_unix_timestamp();
		m_atime = { .tv_sec = current_time, .tv_nsec = 0 };
		m_mtime = { .tv_sec = current_time, .tv_nsec = 0 };
		m_ctime = { .tv_sec = current_time, .tv_nsec = 0 };
	}

	void Pipe::clone_writing()
	{
		LockGuard _(m_lock);
		ASSERT(m_writing_count > 0);
		m_writing_count++;
	}
	
	void Pipe::close_writing()
	{
		LockGuard _(m_lock);
		ASSERT(m_writing_count > 0);
		m_writing_count--;
		if (m_writing_count == 0)
			m_semaphore.unblock();
	}

	BAN::ErrorOr<size_t> Pipe::read(size_t, void* buffer, size_t count)
	{
		m_lock.lock();
		while (m_buffer.empty())
		{
			if (m_writing_count == 0)
				return 0;
			m_lock.unlock();
			m_semaphore.block();
			m_lock.lock();
		}

		size_t to_copy = BAN::Math::min<size_t>(count, m_buffer.size());
		memcpy(buffer, m_buffer.data(), to_copy);

		memmove(m_buffer.data(), m_buffer.data() + to_copy, m_buffer.size() - to_copy);
		MUST(m_buffer.resize(m_buffer.size() - to_copy));

		uint64_t current_time = SystemTimer::get().get_unix_timestamp();
		m_atime = { .tv_sec = current_time, .tv_nsec = 0 };

		m_semaphore.unblock();

		m_lock.unlock();

		return to_copy;
	}
	
	BAN::ErrorOr<size_t> Pipe::write(size_t, const void* buffer, size_t count)
	{
		LockGuard _(m_lock);

		size_t old_size = m_buffer.size();

		TRY(m_buffer.resize(old_size + count));
		memcpy(m_buffer.data() + old_size, buffer, count);

		uint64_t current_time = SystemTimer::get().get_unix_timestamp();
		m_mtime = { .tv_sec = current_time, .tv_nsec = 0 };
		m_ctime = { .tv_sec = current_time, .tv_nsec = 0 };

		m_semaphore.unblock();

		return count;
	}
	
}