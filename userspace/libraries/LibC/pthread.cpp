#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/PlacementNew.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct pthread_trampoline_info_t
{
	void* (*start_routine)(void*);
	void* arg;
};

// stack is 16 byte aligned on entry, this `call` is used to align it
extern "C" void pthread_trampoline(void*);
asm("pthread_trampoline: call pthread_trampoline_cpp");

extern "C" void pthread_trampoline_cpp(void* arg)
{
	pthread_trampoline_info_t info;
	memcpy(&info, arg, sizeof(pthread_trampoline_info_t));
	free(arg);

	pthread_exit(info.start_routine(info.arg));
	ASSERT_NOT_REACHED();
}

int pthread_create(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr, void* (*start_routine)(void*), void* __restrict arg)
{
	auto* info = static_cast<pthread_trampoline_info_t*>(malloc(sizeof(pthread_trampoline_info_t)));
	if (info == nullptr)
		return -1;
	info->start_routine = start_routine;
	info->arg = arg;

	const auto ret = syscall(SYS_PTHREAD_CREATE, attr, pthread_trampoline, info);
	if (ret == -1)
	{
		free(info);
		return -1;
	}

	if (thread)
		*thread = ret;
	return 0;
}

void pthread_exit(void* value_ptr)
{
	syscall(SYS_PTHREAD_EXIT, value_ptr);
	ASSERT_NOT_REACHED();
}

int pthread_join(pthread_t thread, void** value_ptr)
{
	return syscall(SYS_PTHREAD_JOIN, thread, value_ptr);
}

pthread_t pthread_self(void)
{
	return syscall(SYS_PTHREAD_SELF);
}

static inline BAN::Atomic<pthread_t>& pthread_spin_get_atomic(pthread_spinlock_t* lock)
{
	static_assert(sizeof(pthread_spinlock_t) <= sizeof(BAN::Atomic<pthread_t>));
	static_assert(alignof(pthread_spinlock_t) <= alignof(BAN::Atomic<pthread_t>));
	return *reinterpret_cast<BAN::Atomic<pthread_t>*>(lock);
}

int pthread_spin_destroy(pthread_spinlock_t* lock)
{
	pthread_spin_get_atomic(lock).~Atomic<pthread_t>();
	return 0;
}

int pthread_spin_init(pthread_spinlock_t* lock, int pshared)
{
	(void)pshared;
	new (lock) BAN::Atomic<pthread_t>();
	pthread_spin_get_atomic(lock) = false;
	return 0;
}

int pthread_spin_lock(pthread_spinlock_t* lock)
{
	auto& atomic = pthread_spin_get_atomic(lock);

	const pthread_t tid = pthread_self();
	ASSERT(atomic.load(BAN::MemoryOrder::memory_order_relaxed) != tid);

	pthread_t expected = 0;
	while (!atomic.compare_exchange(expected, tid, BAN::MemoryOrder::memory_order_acquire))
	{
		sched_yield();
		expected = 0;
	}

	return 0;
}

int pthread_spin_trylock(pthread_spinlock_t* lock)
{
	auto& atomic = pthread_spin_get_atomic(lock);

	const pthread_t tid = pthread_self();
	ASSERT(atomic.load(BAN::MemoryOrder::memory_order_relaxed) != tid);

	pthread_t expected = 0;
	if (atomic.compare_exchange(expected, tid, BAN::MemoryOrder::memory_order_acquire))
		return 0;
	return EBUSY;
}

int pthread_spin_unlock(pthread_spinlock_t* lock)
{
	auto& atomic = pthread_spin_get_atomic(lock);
	ASSERT(atomic.load(BAN::MemoryOrder::memory_order_relaxed) == pthread_self());
	atomic.store(0, BAN::MemoryOrder::memory_order_release);
	return 0;
}
