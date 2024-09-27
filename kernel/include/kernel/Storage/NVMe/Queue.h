#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Interruptable.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/ThreadBlocker.h>
#include <kernel/Storage/NVMe/Definitions.h>

namespace Kernel
{

	class NVMeQueue : public Interruptable
	{
	public:
		NVMeQueue(BAN::UniqPtr<Kernel::DMARegion>&& cq, BAN::UniqPtr<Kernel::DMARegion>&& sq, volatile NVMe::DoorbellRegisters& db, uint32_t qdepth);

		uint16_t submit_command(NVMe::SubmissionQueueEntry& sqe);

		virtual void handle_irq() final override;

	private:
		uint16_t reserve_cid();

	private:
		BAN::UniqPtr<Kernel::DMARegion> m_completion_queue;
		BAN::UniqPtr<Kernel::DMARegion> m_submission_queue;
		volatile NVMe::DoorbellRegisters& m_doorbell;
		const uint32_t m_qdepth;
		uint32_t m_sq_tail { 0 };
		uint32_t m_cq_head { 0 };
		uint16_t m_cq_valid_phase { 1 };

		ThreadBlocker       m_thread_blocker;
		SpinLock            m_lock;
		BAN::Atomic<size_t> m_used_mask			{ 0 };
		BAN::Atomic<size_t> m_done_mask			{ 0 };
		volatile uint16_t   m_status_codes[64]	{ };

		static constexpr size_t m_mask_bits = sizeof(size_t) * 8;
	};

}
