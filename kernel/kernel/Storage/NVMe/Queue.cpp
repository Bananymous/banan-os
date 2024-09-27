#include <kernel/Lock/LockGuard.h>
#include <kernel/Storage/NVMe/Queue.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static constexpr uint64_t s_nvme_command_timeout_ms = 1000;
	static constexpr uint64_t s_nvme_command_poll_timeout_ms = 20;

	NVMeQueue::NVMeQueue(BAN::UniqPtr<Kernel::DMARegion>&& cq, BAN::UniqPtr<Kernel::DMARegion>&& sq, volatile NVMe::DoorbellRegisters& db, uint32_t qdepth)
		: m_completion_queue(BAN::move(cq))
		, m_submission_queue(BAN::move(sq))
		, m_doorbell(db)
		, m_qdepth(qdepth)
	{
		for (uint32_t i = qdepth; i < m_mask_bits; i++)
			m_used_mask |= (size_t)1 << i;
	}

	void NVMeQueue::handle_irq()
	{
		auto* cq_ptr = reinterpret_cast<NVMe::CompletionQueueEntry*>(m_completion_queue->vaddr());

		while ((cq_ptr[m_cq_head].sts & 1) == m_cq_valid_phase)
		{
			uint16_t sts = cq_ptr[m_cq_head].sts >> 1;
			uint16_t cid = cq_ptr[m_cq_head].cid;
			size_t cid_mask = (size_t)1 << cid;
			ASSERT(cid < m_mask_bits);

			ASSERT((m_done_mask & cid_mask) == 0);

			m_status_codes[cid] = sts;
			m_done_mask |= cid_mask;

			m_cq_head = (m_cq_head + 1) % m_qdepth;
			if (m_cq_head == 0)
				m_cq_valid_phase ^= 1;
		}

		m_doorbell.cq_head = m_cq_head;

		m_thread_blocker.unblock();
	}

	uint16_t NVMeQueue::submit_command(NVMe::SubmissionQueueEntry& sqe)
	{
		uint16_t cid = reserve_cid();
		size_t cid_mask = (size_t)1 << cid;

		{
			SpinLockGuard _(m_lock);

			m_done_mask &= ~cid_mask;
			m_status_codes[cid]	= 0;

			sqe.cid = cid;

			auto* sqe_ptr = reinterpret_cast<NVMe::SubmissionQueueEntry*>(m_submission_queue->vaddr());
			memcpy(&sqe_ptr[m_sq_tail], &sqe, sizeof(NVMe::SubmissionQueueEntry));
			m_sq_tail = (m_sq_tail + 1) % m_qdepth;
			m_doorbell.sq_tail = m_sq_tail;
		}

		const uint64_t start_time_ms = SystemTimer::get().ms_since_boot();
		while (!(m_done_mask & cid_mask) && SystemTimer::get().ms_since_boot() < start_time_ms + s_nvme_command_poll_timeout_ms)
			continue;

		// FIXME: Here is a possible race condition if done mask is set before
		//        scheduler has put the current thread blocking.
		//        EINTR should also be handled here.
		while (!(m_done_mask & cid_mask) && SystemTimer::get().ms_since_boot() < start_time_ms + s_nvme_command_timeout_ms)
			m_thread_blocker.block_with_wake_time_ms(start_time_ms + s_nvme_command_timeout_ms);

		if (m_done_mask & cid_mask)
		{
			uint16_t status = m_status_codes[cid];
			m_used_mask &= ~cid_mask;
			return status;
		}

		m_used_mask &= ~cid_mask;
		return 0xFFFF;
	}

	uint16_t NVMeQueue::reserve_cid()
	{
		auto state = m_lock.lock();
		while (~m_used_mask == 0)
		{
			m_lock.unlock(state);
			m_thread_blocker.block_with_timeout_ms(s_nvme_command_timeout_ms);
			state = m_lock.lock();
		}

		uint16_t cid = 0;
		for (; cid < m_mask_bits; cid++)
			if ((m_used_mask & ((size_t)1 << cid)) == 0)
				break;
		ASSERT(cid < m_mask_bits);
		ASSERT(cid < m_qdepth);

		m_used_mask |= (size_t)1 << cid;

		m_lock.unlock(state);
		return cid;
	}

}
