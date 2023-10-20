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
		timespec current_time = SystemTimer::get().real_time();
		m_atime = current_time;
		m_mtime = current_time;
		m_ctime = current_time;
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

	BAN::ErrorOr<size_t> Pipe::read_impl(off_t, BAN::ByteSpan buffer)
	{
		LockGuard _(m_lock);
		while (m_buffer.empty())
		{
			if (m_writing_count == 0)
				return 0;
			m_lock.unlock();
			m_semaphore.block();
			m_lock.lock();
		}

		size_t to_copy = BAN::Math::min<size_t>(buffer.size(), m_buffer.size());
		memcpy(buffer.data(), m_buffer.data(), to_copy);

		memmove(m_buffer.data(), m_buffer.data() + to_copy, m_buffer.size() - to_copy);
		MUST(m_buffer.resize(m_buffer.size() - to_copy));

		m_atime = SystemTimer::get().real_time();

		m_semaphore.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> Pipe::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		LockGuard _(m_lock);

		size_t old_size = m_buffer.size();

		TRY(m_buffer.resize(old_size + buffer.size()));
		memcpy(m_buffer.data() + old_size, buffer.data(), buffer.size());

		timespec current_time = SystemTimer::get().real_time();
		m_mtime = current_time;
		m_ctime = current_time;

		m_semaphore.unblock();

		return buffer.size();
	}

}