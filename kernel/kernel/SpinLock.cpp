#include <kernel/SpinLock.h>

namespace Kernel
{

	extern "C" void spinlock_lock_asm(int*);
	extern "C" void spinlock_unlock_asm(int*);

	void SpinLock::lock()
	{
		spinlock_lock_asm(&m_lock);
	}

	void SpinLock::unlock()
	{
		spinlock_unlock_asm(&m_lock);
	}

	bool SpinLock::is_locked() const
	{
		return m_lock;
	}

}