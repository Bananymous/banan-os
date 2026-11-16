#include <BAN/HashMap.h>
#include <kernel/FS/Pipe.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

namespace Kernel
{

	static constexpr size_t s_pipe_buffer_size = 0x10000;

	static Mutex s_named_pipe_mutex;
	static BAN::HashMap<BAN::RefPtr<Inode>, BAN::WeakPtr<Pipe>> s_named_pipes;

	BAN::ErrorOr<BAN::RefPtr<Inode>> Pipe::open(BAN::RefPtr<Inode> inode, int status_flags)
	{
		BAN::RefPtr<Pipe> pipe;

		{
			LockGuard _(s_named_pipe_mutex);

			auto it = s_named_pipes.find(inode);
			if (it == s_named_pipes.end())
				it = TRY(s_named_pipes.insert(inode, {}));

			if (!(pipe = it->value.lock()))
			{
				// FIXME: these should probably reference the underlying inode(?)
				const struct stat st {
					.st_dev     = inode->dev(),
					.st_ino     = inode->ino(),
					.st_mode    = inode->mode().mode,
					.st_nlink   = inode->nlink(),
					.st_uid     = inode->uid(),
					.st_gid     = inode->gid(),
					.st_rdev    = inode->rdev(),
					.st_size    = inode->size(),
					.st_atim    = inode->atime(),
					.st_mtim    = inode->mtime(),
					.st_ctim    = inode->ctime(),
					.st_blksize = inode->blksize(),
					.st_blocks  = inode->blocks(),
				};

				auto* pipe_ptr = new Pipe(st);
				if (pipe_ptr == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				pipe = BAN::RefPtr<Pipe>::adopt(pipe_ptr);
				pipe->m_buffer = TRY(ByteRingBuffer::create(s_pipe_buffer_size));
				pipe->m_named_inode = inode;

				it->value = TRY(pipe->get_weak_ptr());
			}
		}

		LockGuard _(pipe->m_mutex);

		if (status_flags & O_RDONLY)
			pipe->m_reading_count++;
		if (status_flags & O_WRONLY)
			pipe->m_writing_count++;

		if (status_flags & O_NONBLOCK)
		{
			if ((status_flags & O_WRONLY) && pipe->m_reading_count == 0)
				return BAN::Error::from_errno(ENXIO);
			return BAN::RefPtr<Inode>(pipe);
		}

		auto& block_value = (status_flags & O_WRONLY) ? pipe->m_reading_count : pipe->m_writing_count;
		while (block_value == 0)
			TRY(Thread::current().block_or_eintr_indefinite(pipe->m_thread_blocker, &pipe->m_mutex));
		return BAN::RefPtr<Inode>(pipe);
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Pipe::create(uid_t uid, gid_t gid)
	{
		const timespec current_time = SystemTimer::get().real_time();
		const struct stat st {
			.st_dev     = 0, // FIXME
			.st_ino     = 0, // FIXME
			.st_mode    = Mode::IFIFO | Mode::IRUSR | Mode::IWUSR,
			.st_nlink   = 0,
			.st_uid     = uid,
			.st_gid     = gid,
			.st_rdev    = 0, // FIXME
			.st_size    = 0,
			.st_atim    = current_time,
			.st_mtim    = current_time,
			.st_ctim    = current_time,
			.st_blksize = PAGE_SIZE,
			.st_blocks  = 0,
		};

		auto* pipe_ptr = new Pipe(st);
		if (pipe_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto pipe = BAN::RefPtr<Pipe>::adopt(pipe_ptr);
		pipe->m_buffer = TRY(ByteRingBuffer::create(s_pipe_buffer_size));
		pipe->m_reading_count++;
		pipe->m_writing_count++;
		return BAN::RefPtr<Inode>(pipe);
	}

	Pipe::Pipe(const struct stat& st)
	{
		m_ino     = st.st_ino;
		m_mode    = st.st_mode;
		m_nlink   = st.st_nlink;
		m_uid     = st.st_uid;
		m_gid     = st.st_gid;
		m_size    = st.st_size;
		m_atime   = st.st_atim;
		m_mtime   = st.st_mtim;
		m_ctime   = st.st_ctim;
		m_blksize = st.st_blksize;
		m_blocks  = st.st_blocks;
		m_dev     = st.st_dev;
		m_rdev    = st.st_rdev;

		m_kind |= InodeKind::PIPE;
	}

	Pipe::~Pipe()
	{
		if (!m_named_inode)
			return;
		LockGuard _(s_named_pipe_mutex);
		s_named_pipes.remove(m_named_inode);
	}

	void Pipe::on_clone(int status_flags)
	{
		if (status_flags & O_WRONLY)
			m_writing_count++;

		if (status_flags & O_RDONLY)
			m_reading_count++;
	}

	void Pipe::on_close(int status_flags)
	{
		LockGuard _(m_mutex);

		if (status_flags & O_WRONLY)
			if (--m_writing_count == 0)
				epoll_notify(EPOLLHUP);

		if (status_flags & O_RDONLY)
			if (--m_reading_count == 0)
				epoll_notify(EPOLLERR);

		m_thread_blocker.unblock();
	}

	BAN::ErrorOr<void> Pipe::sync_inode(SyncType type)
	{
		if (!m_named_inode)
			return {};

		switch (type)
		{
			case SyncType::General:
				break;
			case SyncType::Mode:
				TRY(m_named_inode->chmod(m_mode));
				break;
			case SyncType::UidGid:
				TRY(m_named_inode->chown(m_uid, m_gid));
				break;
			case SyncType::Times:
				const timespec times[] { m_atime, m_mtime };
				TRY(m_named_inode->utimens(times));
				break;
		}

		return {};
	}

	BAN::ErrorOr<void> Pipe::sync_data()
	{
		return {};
	}

	BAN::ErrorOr<size_t> Pipe::read_impl(off_t, BAN::ByteSpan buffer)
	{
		LockGuard _(m_mutex);

		while (m_buffer->empty())
		{
			if (m_writing_count == 0)
				return 0;
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		const size_t to_copy = BAN::Math::min<size_t>(buffer.size(), m_buffer->size());
		memcpy(buffer.data(), m_buffer->get_data().data(), to_copy);
		m_buffer->pop(to_copy);

		epoll_notify(EPOLLOUT);

		m_thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> Pipe::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		LockGuard _(m_mutex);

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

		epoll_notify(EPOLLIN);

		m_thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<void> Pipe::truncate_impl(size_t)
	{
		return BAN::Error::from_errno(ENODEV);
	}

}
