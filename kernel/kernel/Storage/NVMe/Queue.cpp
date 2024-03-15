#include <kernel/Lock/LockGuard.h>
#include <kernel/Scheduler.h>
#include <kernel/Storage/NVMe/Queue.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static constexpr uint64_t s_nvme_command_timeout_ms = 1000;
	static constexpr uint64_t s_nvme_command_poll_timeout_ms = 20;

	NVMeQueue::NVMeQueue(BAN::UniqPtr<Kernel::DMARegion>&& cq, BAN::UniqPtr<Kernel::DMARegion>&& sq, volatile NVMe::DoorbellRegisters& db, uint32_t qdepth, uint8_t irq)
		: m_completion_queue(BAN::move(cq))
		, m_submission_queue(BAN::move(sq))
		, m_doorbell(db)
		, m_qdepth(qdepth)
	{
		for (uint32_t i = qdepth; i < 64; i++)
			m_used_mask |= (uint64_t)1 << i;
		set_irq(irq);
		enable_interrupt();
	}

	void NVMeQueue::handle_irq()
	{
		auto* cq_ptr = reinterpret_cast<NVMe::CompletionQueueEntry*>(m_completion_queue->vaddr());

		while ((cq_ptr[m_cq_head].sts & 1) == m_cq_valid_phase)
		{
			uint16_t sts = cq_ptr[m_cq_head].sts >> 1;
			uint16_t cid = cq_ptr[m_cq_head].cid;
			uint64_t cid_mask = (uint64_t)1 << cid;
			ASSERT(cid < 64);

			ASSERT((m_done_mask & cid_mask) == 0);

			m_status_codes[cid] = sts;
			m_done_mask |= cid_mask;

			m_cq_head = (m_cq_head + 1) % m_qdepth;
			if (m_cq_head == 0)
				m_cq_valid_phase ^= 1;
		}

		m_doorbell.cq_head = m_cq_head;

		m_semaphore.unblock();
	}

	uint16_t NVMeQueue::submit_command(NVMe::SubmissionQueueEntry& sqe)
	{
		uint16_t cid = reserve_cid();
		uint64_t cid_mask = (uint64_t)1 << cid;

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

		const uint64_t start_time = SystemTimer::get().ms_since_boot();
		while (SystemTimer::get().ms_since_boot() < start_time + s_nvme_command_poll_timeout_ms)
		{
			if (m_done_mask & cid_mask)
			{
				uint16_t status = m_status_codes[cid];
				m_used_mask &= ~cid_mask;
				return status;
			}
		}

		while (SystemTimer::get().ms_since_boot() < start_time + s_nvme_command_timeout_ms)
		{
			if (m_done_mask & cid_mask)
			{
				uint16_t status = m_status_codes[cid];
				m_used_mask &= ~cid_mask;
				return status;
			}
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
			m_semaphore.block_with_timeout(s_nvme_command_timeout_ms);
			state = m_lock.lock();
		}

		uint16_t cid = 0;
		for (; cid < 64; cid++)
			if ((m_used_mask & ((uint64_t)1 << cid)) == 0)
				break;
		ASSERT(cid < 64);
		ASSERT(cid < m_qdepth);

		m_used_mask |= (uint64_t)1 << cid;

		m_lock.unlock(state);
		return cid;
	}

}
