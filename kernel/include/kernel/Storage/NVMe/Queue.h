#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/Semaphore.h>
#include <kernel/Storage/NVMe/Definitions.h>

namespace Kernel
{

	class NVMeQueue : public Interruptable
	{
	public:
		NVMeQueue(BAN::UniqPtr<Kernel::DMARegion>&& cq, BAN::UniqPtr<Kernel::DMARegion>&& sq, volatile NVMe::DoorbellRegisters& db, uint32_t qdepth, uint8_t irq);

		uint16_t submit_command(NVMe::SubmissionQueueEntry& sqe);

		virtual void handle_irq() final override;

	private:
		SpinLock m_lock;
		BAN::UniqPtr<Kernel::DMARegion> m_completion_queue;
		BAN::UniqPtr<Kernel::DMARegion> m_submission_queue;
		volatile NVMe::DoorbellRegisters& m_doorbell;
		const uint32_t m_qdepth;
		uint32_t m_sq_tail { 0 };
		uint32_t m_cq_head { 0 };
		uint16_t m_cq_valid_phase { 1 };

		Semaphore			m_semaphore;
		volatile uint16_t	m_status;
		volatile bool		m_done { false };
	};

}
