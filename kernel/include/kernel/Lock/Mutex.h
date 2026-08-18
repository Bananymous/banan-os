#pragma once

#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>

#include <sys/types.h>

namespace Kernel
{

	class BaseMutex
	{
	public:
		virtual void lock() = 0;
		virtual void unlock() = 0;

		virtual uint32_t lock_depth() const = 0;
		virtual bool is_locked_by_current_thread() const = 0;
	};

	class Mutex final : public BaseMutex
	{
		BAN_NON_COPYABLE(Mutex);
		BAN_NON_MOVABLE(Mutex);

	public:
		Mutex() = default;

		bool try_lock();
		void lock() override;
		void unlock() override;

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const override { return m_lock_depth; }
		bool is_locked_by_current_thread() const override;

	private:
		BAN::Atomic<pid_t>	m_locker		{ -1 };
		uint32_t			m_lock_depth	{  0 };
	};

	class PriorityMutex final : public BaseMutex
	{
		BAN_NON_COPYABLE(PriorityMutex);
		BAN_NON_MOVABLE(PriorityMutex);

	public:
		PriorityMutex() = default;

		bool try_lock();
		void lock() override;
		void unlock() override;

		pid_t locker() const { return m_locker; }
		bool is_locked() const { return m_locker != -1; }
		uint32_t lock_depth() const override { return m_lock_depth; }
		bool is_locked_by_current_thread() const override;

	private:
		BAN::Atomic<pid_t>		m_locker		{ -1 };
		uint32_t				m_lock_depth	{  0 };
		BAN::Atomic<uint32_t>	m_queue_length	{  0 };
	};

}
