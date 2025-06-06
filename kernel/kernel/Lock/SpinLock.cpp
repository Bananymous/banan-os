#include <kernel/Lock/SpinLock.h>
#include <kernel/Thread.h>

namespace Kernel
{

	InterruptState SpinLock::lock()
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		auto id = Processor::current_id().as_u32();
		ASSERT(m_locker.load(BAN::MemoryOrder::memory_order_relaxed) != id);

		auto expected = PROCESSOR_NONE.as_u32();
		while (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acquire))
		{
			Processor::pause();
			expected = PROCESSOR_NONE.as_u32();
		}

		if (Thread::current_tid())
			Thread::current().add_spinlock();

		return state;
	}

	bool SpinLock::try_lock_interrupts_disabled()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		auto id = Processor::current_id().as_u32();
		ASSERT(m_locker.load(BAN::MemoryOrder::memory_order_relaxed) != id);

		auto expected = PROCESSOR_NONE.as_u32();
		if (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acquire))
			return false;

		if (Thread::current_tid())
			Thread::current().add_spinlock();

		return true;
	}

	void SpinLock::unlock(InterruptState state)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		ASSERT(current_processor_has_lock());
		m_locker.store(PROCESSOR_NONE.as_u32(), BAN::MemoryOrder::memory_order_release);
		if (Thread::current_tid())
			Thread::current().remove_spinlock();
		Processor::set_interrupt_state(state);
	}

	InterruptState RecursiveSpinLock::lock()
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		auto id = Processor::current_id().as_u32();

		ProcessorID::value_type expected = PROCESSOR_NONE.as_u32();
		while (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acq_rel))
		{
			if (expected == id)
				break;
			Processor::pause();
			expected = PROCESSOR_NONE.as_u32();
		}

		m_lock_depth++;

		if (Thread::current_tid())
			Thread::current().add_spinlock();

		return state;
	}

	bool RecursiveSpinLock::try_lock_interrupts_disabled()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		auto id = Processor::current_id().as_u32();

		ProcessorID::value_type expected = PROCESSOR_NONE.as_u32();
		if (!m_locker.compare_exchange(expected, id, BAN::MemoryOrder::memory_order_acq_rel))
			if (expected != id)
				return false;

		m_lock_depth++;

		if (Thread::current_tid())
			Thread::current().add_spinlock();

		return true;
	}

	void RecursiveSpinLock::unlock(InterruptState state)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		ASSERT(current_processor_has_lock());
		ASSERT(m_lock_depth > 0);
		if (--m_lock_depth == 0)
			m_locker.store(PROCESSOR_NONE.as_u32(), BAN::MemoryOrder::memory_order_release);
		if (Thread::current_tid())
			Thread::current().remove_spinlock();
		Processor::set_interrupt_state(state);
	}

}
