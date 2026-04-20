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
		SpinLockGuard _(m_lock);

		for (auto* node = m_block_chain; node;)
		{
			auto* next = node->block_chain_next;

			ASSERT(node->blocked);
			ASSERT(node->blocker == this);

			node->blocker.store(nullptr);
			node->block_chain_prev = nullptr;
			node->block_chain_next = nullptr;

			Processor::scheduler().unblock_thread(node);

			node = next;
		}

		m_block_chain = nullptr;
	}

	void ThreadBlocker::add_thread_to_block_queue(SchedulerQueue::Node* node)
	{
		SpinLockGuard _(m_lock);

		ASSERT(node->blocked);
		ASSERT(node->blocker == nullptr);

		node->blocker.store(this);
		node->block_chain_prev = nullptr;
		node->block_chain_next = m_block_chain;

		if (m_block_chain)
			m_block_chain->block_chain_prev = node;
		m_block_chain = node;
	}

	void ThreadBlocker::remove_thread_from_block_queue(SchedulerQueue::Node* node)
	{
		SpinLockGuard _(m_lock);

		// NOTE: this is possible if we got here while another
		//       core was doing an unblock on this blocker
		if (node->blocker.load() != this)
			return;
		ASSERT(node->blocked);

		if (node->block_chain_prev)
			node->block_chain_prev->block_chain_next = node->block_chain_next;
		if (node->block_chain_next)
			node->block_chain_next->block_chain_prev = node->block_chain_prev;

		if (node == m_block_chain)
			m_block_chain = node->block_chain_next;

		node->blocker.store(nullptr);
		node->block_chain_prev = nullptr;
		node->block_chain_next = nullptr;
	}

}
