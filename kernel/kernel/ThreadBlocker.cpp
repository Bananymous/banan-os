#include <kernel/Processor.h>
#include <kernel/SchedulerQueueNode.h>
#include <kernel/ThreadBlocker.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	void ThreadBlocker::block_indefinite(BaseMutex* mutex)
	{
		Processor::scheduler().block_current_thread(this, static_cast<uint64_t>(-1), mutex);
	}

	void ThreadBlocker::block_with_timeout_ns(uint64_t timeout_ns, BaseMutex* mutex)
	{
		Processor::scheduler().block_current_thread(this, SystemTimer::get().ns_since_boot() + timeout_ns, mutex);
	}

	void ThreadBlocker::block_with_wake_time_ns(uint64_t wake_time_ns, BaseMutex* mutex)
	{
		Processor::scheduler().block_current_thread(this, wake_time_ns, mutex);
	}

	void ThreadBlocker::unblock()
	{
		decltype(m_block_chain) temp_block_chain;
		size_t temp_block_chain_length { 0 };

		{
			SpinLockGuard _(m_lock);
			for (size_t i = 0; i < m_block_chain_length; i++)
				temp_block_chain[i] = m_block_chain[i];
			temp_block_chain_length = m_block_chain_length;
			m_block_chain_length = 0;
		}

		for (size_t i = 0; i < temp_block_chain_length; i++)
			Processor::scheduler().unblock_thread(temp_block_chain[i]);
	}

	void ThreadBlocker::add_thread_to_block_queue(SchedulerQueue::Node* node)
	{
		ASSERT(node->blocker_lock.current_processor_has_lock());

		SpinLockGuard _(m_lock);

		ASSERT(m_block_chain_length < sizeof(m_block_chain) / sizeof(m_block_chain[0]));

		ASSERT(node);
		ASSERT(node->blocked);
		ASSERT(node->blocker == nullptr);

		for (size_t i = 0 ; i < m_block_chain_length; i++)
			ASSERT(m_block_chain[i] != node);
		m_block_chain[m_block_chain_length++] = node;

		node->blocker = this;
	}

	void ThreadBlocker::remove_blocked_thread(SchedulerQueue::Node* node)
	{
		ASSERT(node->blocker_lock.current_processor_has_lock());

		SpinLockGuard _(m_lock);

		ASSERT(node);
		ASSERT(node->blocked);
		ASSERT(node->blocker == this);

		for (size_t i = 0 ; i < m_block_chain_length; i++)
		{
			if (m_block_chain[i] != node)
				continue;
			for (size_t j = i + 1; j < m_block_chain_length; j++)
				m_block_chain[j - 1] = m_block_chain[j];
			m_block_chain_length--;
		}

		node->blocker = nullptr;
	}

}
