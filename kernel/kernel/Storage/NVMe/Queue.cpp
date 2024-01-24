#include <kernel/LockGuard.h>
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
			ASSERT(cid == 0);

			ASSERT(!m_done);
			m_status = sts;
			m_done = true;
			m_semaphore.unblock();

			m_cq_head = (m_cq_head + 1) % m_qdepth;
			if (m_cq_head == 0)
				m_cq_valid_phase ^= 1;
		}

		m_doorbell.cq_head = m_cq_head;
	}

	uint16_t NVMeQueue::submit_command(NVMe::SubmissionQueueEntry& sqe)
	{
		LockGuard _(m_lock);

		ASSERT(m_done == false);
		m_status = 0;

		sqe.cid = 0;

		auto* sqe_ptr = reinterpret_cast<NVMe::SubmissionQueueEntry*>(m_submission_queue->vaddr());
		memcpy(&sqe_ptr[m_sq_tail], &sqe, sizeof(NVMe::SubmissionQueueEntry));
		m_sq_tail = (m_sq_tail + 1) % m_qdepth;
		m_doorbell.sq_tail = m_sq_tail;

		const uint64_t start_time = SystemTimer::get().ms_since_boot();
		while (SystemTimer::get().ms_since_boot() < start_time + s_nvme_command_poll_timeout_ms)
		{
			if (!m_done)
				continue;
			m_done = false;
			return m_status;
		}

		while (SystemTimer::get().ms_since_boot() < start_time + s_nvme_command_timeout_ms)
		{
			if (!m_done)
			{
				m_semaphore.block();
				continue;
			}
			m_done = false;
			return m_status;
		}

		return 0xFFFF;
	}

}
