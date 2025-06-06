#include <kernel/Processor.h>
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
		SchedulerQueue::Node* block_chain;

		{
			SpinLockGuard _(m_lock);
			block_chain = m_block_chain;
			m_block_chain = nullptr;
		}

		for (auto* node = block_chain; node;)
		{
			ASSERT(node->blocked);

			auto* next = node->block_chain_next;
			node->blocker = nullptr;
			node->block_chain_next = nullptr;
			node->block_chain_prev = nullptr;
			Processor::scheduler().unblock_thread(node);
			node = next;
		}
	}

	void ThreadBlocker::add_thread_to_block_queue(SchedulerQueue::Node* node)
	{
		ASSERT(node);
		ASSERT(node->blocked);
		ASSERT(node->blocker == nullptr);
		ASSERT(node->block_chain_prev == nullptr);
		ASSERT(node->block_chain_next == nullptr);

		SpinLockGuard _(m_lock);
		node->blocker = this;
		node->block_chain_prev = nullptr;
		node->block_chain_next = m_block_chain;
		if (m_block_chain)
			m_block_chain->block_chain_prev = node;
		m_block_chain = node;
	}

	void ThreadBlocker::remove_blocked_thread(SchedulerQueue::Node* node)
	{
		SpinLockGuard _(m_lock);

		ASSERT(node);
		ASSERT(node->blocked);
		ASSERT(node->blocker == this);

		if (node == m_block_chain)
		{
			ASSERT(node->block_chain_prev == nullptr);
			m_block_chain = node->block_chain_next;
			if (m_block_chain)
				m_block_chain->block_chain_prev = nullptr;
		}
		else
		{
			ASSERT(node->block_chain_prev);
			node->block_chain_prev->block_chain_next = node->block_chain_next;
			if (node->block_chain_next)
				node->block_chain_next->block_chain_prev = node->block_chain_prev;
		}

		node->blocker = nullptr;
		node->block_chain_next = nullptr;
		node->block_chain_prev = nullptr;
	}


}
