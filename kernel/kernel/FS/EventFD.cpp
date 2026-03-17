#include <kernel/FS/EventFD.h>

#include <sys/epoll.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Inode>> EventFD::create(uint64_t initval, bool semaphore)
	{
		auto* eventfd_ptr = new EventFD(initval, semaphore);
		if (eventfd_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<Inode>(BAN::RefPtr<EventFD>::adopt(eventfd_ptr));
	}

	BAN::ErrorOr<size_t> EventFD::read_impl(off_t, BAN::ByteSpan buffer)
	{
		if (buffer.size() < sizeof(uint64_t))
			return BAN::Error::from_errno(EINVAL);

		while (m_value == 0)
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));

		const uint64_t read_value = m_is_semaphore ? 1 : m_value;
		m_value -= read_value;

		buffer.as<uint64_t>() = read_value;

		epoll_notify(EPOLLOUT);

		return sizeof(uint64_t);
	}

	BAN::ErrorOr<size_t> EventFD::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		if (buffer.size() < sizeof(uint64_t))
			return BAN::Error::from_errno(EINVAL);

		const uint64_t write_value = buffer.as<const uint64_t>();
		if (write_value == UINT64_MAX)
			return BAN::Error::from_errno(EINVAL);

		while (m_value + write_value < m_value)
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));

		m_value += write_value;

		if (m_value > 0)
			epoll_notify(EPOLLIN);

		return sizeof(uint64_t);
	}

}
