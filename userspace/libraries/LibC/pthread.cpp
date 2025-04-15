#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/PlacementNew.h>

#include <kernel/Arch.h>

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
extern "C" void _pthread_trampoline(void*);
asm(
#if ARCH(x86_64)
"_pthread_trampoline:"
	"popq %rdi;"
	"andq $-16, %rsp;"
	"xorq %rbp, %rbp;"
	"call _pthread_trampoline_cpp"
#elif ARCH(i686)
"_pthread_trampoline:"
	"ud2;"
	"popl %edi;"
	"andl $-16, %esp;"
	"xorl %ebp, %ebp;"
	"subl $12, %esp;"
	"pushl %edi;"
	"call _pthread_trampoline_cpp"
#endif
);

extern "C" void _pthread_trampoline_cpp(void* arg)
{
	auto info = *reinterpret_cast<pthread_trampoline_info_t*>(arg);
	free(arg);
	pthread_exit(info.start_routine(info.arg));
	ASSERT_NOT_REACHED();
}

struct pthread_cleanup_t
{
	void (*routine)(void*);
	void* arg;
	pthread_cleanup_t* next;
};

static thread_local pthread_cleanup_t* s_cleanup_stack = nullptr;

void pthread_cleanup_pop(int execute)
{
	ASSERT(s_cleanup_stack);

	auto* cleanup = s_cleanup_stack;
	s_cleanup_stack = cleanup->next;

	if (execute)
		cleanup->routine(cleanup->arg);

	free(cleanup);
}

void pthread_cleanup_push(void (*routine)(void*), void* arg)
{
	auto* cleanup = static_cast<pthread_cleanup_t*>(malloc(sizeof(pthread_cleanup_t)));
	ASSERT(cleanup);

	cleanup->routine = routine;
	cleanup->arg = arg;
	cleanup->next = s_cleanup_stack;

	s_cleanup_stack = cleanup;
}

int pthread_create(pthread_t* __restrict thread_id, const pthread_attr_t* __restrict attr, void* (*start_routine)(void*), void* __restrict arg)
{
	auto* info = static_cast<pthread_trampoline_info_t*>(malloc(sizeof(pthread_trampoline_info_t)));
	if (info == nullptr)
		return errno;

	*info = {
		.start_routine = start_routine,
		.arg = arg,
	};

	const auto ret = syscall(SYS_PTHREAD_CREATE, attr, pthread_trampoline, info);
	if (ret == -1)
		goto pthread_create_error;


	if (thread_id)
		*thread_id = ret;
	return 0;

pthread_create_error:
	const int return_code = errno;
	free(info);
	return return_code;
}

void pthread_exit(void* value_ptr)
{
	while (s_cleanup_stack)
		pthread_cleanup_pop(1);
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
