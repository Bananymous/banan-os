#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/Debug.h>
#include <BAN/PlacementNew.h>

#include <kernel/Arch.h>
#include <kernel/Thread.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

struct pthread_trampoline_info_t
{
	struct uthread* uthread;
	void* (*start_routine)(void*);
	void* arg;
};

static constexpr unsigned rwlock_writer_locked = -1;

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
	info.uthread->id = syscall(SYS_PTHREAD_SELF);
	syscall(SYS_SET_TLS, info.uthread);
	free(arg);
	pthread_exit(info.start_routine(info.arg));
	ASSERT_NOT_REACHED();
}

static void free_uthread(uthread* uthread)
{
	if (uthread->dtv[0] == 0)
		return free(uthread);

	uint8_t* tls_addr = reinterpret_cast<uint8_t*>(uthread) - uthread->master_tls_size;
	const size_t tls_size = uthread->master_tls_size
		+ sizeof(struct uthread)
		+ (uthread->dtv[0] + 1) * sizeof(uintptr_t);
	munmap(tls_addr, tls_size);
}

void pthread_cleanup_pop(int execute)
{
	uthread* uthread = _get_uthread();
	ASSERT(uthread->cleanup_stack);

	auto* cleanup = uthread->cleanup_stack;
	uthread->cleanup_stack = cleanup->next;

	if (execute)
		cleanup->routine(cleanup->arg);

	free(cleanup);
}

void pthread_cleanup_push(void (*routine)(void*), void* arg)
{
	auto* cleanup = static_cast<_pthread_cleanup_t*>(malloc(sizeof(_pthread_cleanup_t)));
	ASSERT(cleanup);

	uthread* uthread = _get_uthread();

	cleanup->routine = routine;
	cleanup->arg = arg;
	cleanup->next = uthread->cleanup_stack;

	uthread->cleanup_stack = cleanup;
}

static thread_local struct
{
	void* value;
	pthread_key_t key;
} s_pthread_key_values[PTHREAD_KEYS_MAX] {};

static pthread_key_t s_pthread_key_current = 1;
static pthread_key_t s_pthread_key_map[PTHREAD_KEYS_MAX] {};
static void (*s_pthread_key_destructors[PTHREAD_KEYS_MAX])(void*) {};
static pthread_spinlock_t s_pthread_key_lock = PTHREAD_SPIN_INITIALIZER;

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
	int ret = EAGAIN;

	pthread_spin_lock(&s_pthread_key_lock);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i])
			continue;
		s_pthread_key_destructors[i] = destructor;
		s_pthread_key_map[i] = *key = s_pthread_key_current++;
		ret = 0;
		break;
	}
	pthread_spin_unlock(&s_pthread_key_lock);

	return ret;
}

int pthread_key_delete(pthread_key_t key)
{
	int ret = EINVAL;

	pthread_spin_lock(&s_pthread_key_lock);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		s_pthread_key_destructors[i] = nullptr;
		s_pthread_key_map[i] = 0;
		ret = 0;
		break;
	}
	pthread_spin_unlock(&s_pthread_key_lock);

	return ret;
}

void* pthread_getspecific(pthread_key_t key)
{
	void* ret = nullptr;

	pthread_spin_lock(&s_pthread_key_lock);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		if (s_pthread_key_values[i].key != key)
		{
			s_pthread_key_values[i].key = key;
			s_pthread_key_values[i].value = nullptr;
		}
		ret = s_pthread_key_values[i].value;
		break;
	}
	pthread_spin_unlock(&s_pthread_key_lock);

	return ret;
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
	int ret = EINVAL;

	pthread_spin_lock(&s_pthread_key_lock);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		if (s_pthread_key_values[i].key != key)
			s_pthread_key_values[i].key = key;
		s_pthread_key_values[i].value = const_cast<void*>(value);
		ret = 0;
		break;
	}
	pthread_spin_unlock(&s_pthread_key_lock);

	return ret;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	*attr = {
		.inheritsched = PTHREAD_INHERIT_SCHED,
		.schedparam = {},
		.schedpolicy = SCHED_RR,
		.detachstate = PTHREAD_CREATE_JOINABLE,
		.scope = PTHREAD_SCOPE_SYSTEM,
		.stacksize = Kernel::Thread::userspace_stack_size,
		.guardsize = static_cast<size_t>(getpagesize()),
	};
	return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate)
{
	*detachstate = attr->detachstate;
	return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
	switch (detachstate)
	{
		case PTHREAD_CREATE_DETACHED:
			dwarnln("TODO: pthread_attr_setdetachstate");
			return ENOTSUP;
		case PTHREAD_CREATE_JOINABLE:
			attr->detachstate = detachstate;
			return 0;
	}
	return EINVAL;
}

int pthread_attr_getguardsize(const pthread_attr_t* __restrict attr, size_t* __restrict guardsize)
{
	*guardsize = attr->guardsize;
	return 0;
}

int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize)
{
	attr->guardsize = guardsize;
	return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t* __restrict attr, int* __restrict inheritsched)
{
	*inheritsched = attr->inheritsched;
	return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t* attr, int inheritsched)
{
	switch (inheritsched)
	{
		case PTHREAD_INHERIT_SCHED:
		case PTHREAD_EXPLICIT_SCHED:
			attr->inheritsched = inheritsched;
			return 0;
	}
	return EINVAL;
}

int pthread_attr_getschedparam(const pthread_attr_t* __restrict attr, struct sched_param* __restrict param)
{
	*param = attr->schedparam;
	return 0;
}

int pthread_attr_setschedparam(pthread_attr_t* __restrict attr, const struct sched_param* __restrict param)
{
	attr->schedparam = *param;
	return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t* __restrict attr, int* __restrict policy)
{
	*policy = attr->schedpolicy;
	return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
	switch (policy)
	{
		case SCHED_FIFO:
		case SCHED_SPORADIC:
		case SCHED_OTHER:
			return ENOTSUP;
		case SCHED_RR:
			attr->schedpolicy = policy;
			return 0;
	}
	return EINVAL;
}

int pthread_attr_getscope(const pthread_attr_t* __restrict attr, int* __restrict contentionscope)
{
	*contentionscope = attr->scope;
	return 0;
}

int pthread_attr_setscope(pthread_attr_t* attr, int contentionscope)
{
	switch (contentionscope)
	{
		case PTHREAD_SCOPE_PROCESS:
			return ENOTSUP;
		case PTHREAD_SCOPE_SYSTEM:
			attr->scope = contentionscope;
			return 0;
	}
	return EINVAL;
}

int pthread_attr_getstack(const pthread_attr_t* __restrict attr, void** __restrict stackaddr, size_t* __restrict stacksize)
{
	(void)attr;
	(void)stackaddr;
	(void)stacksize;
	dwarnln("TODO: pthread_attr_getstack");
	return ENOTSUP;
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize)
{
	(void)attr;
	(void)stackaddr;
	(void)stacksize;
	dwarnln("TODO: pthread_attr_setstack");
	return ENOTSUP;
}

int pthread_attr_getstacksize(const pthread_attr_t* __restrict attr, size_t* __restrict stacksize)
{
	*stacksize = attr->stacksize;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize)
{
	attr->stacksize = stacksize;
	return 0;
}

int pthread_create(pthread_t* __restrict thread_id, const pthread_attr_t* __restrict attr, void* (*start_routine)(void*), void* __restrict arg)
{
	auto* info = static_cast<pthread_trampoline_info_t*>(malloc(sizeof(pthread_trampoline_info_t)));
	if (info == nullptr)
		return errno;

	*info = {
		.uthread = nullptr,
		.start_routine = start_routine,
		.arg = arg,
	};

	long syscall_ret = 0;

	if (uthread* self = _get_uthread(); self->master_tls_addr == nullptr)
	{
		uthread* uthread = static_cast<struct uthread*>(malloc(sizeof(struct uthread) + sizeof(uintptr_t)));
		if (uthread == nullptr)
			goto pthread_create_error;

		*uthread = {
			.self = uthread,
			.master_tls_addr = nullptr,
			.master_tls_size = 0,
			.cleanup_stack = nullptr,
			.id = -1,
			.errno_ = 0,
			.cancel_type = PTHREAD_CANCEL_DEFERRED,
			.cancel_state = PTHREAD_CANCEL_ENABLE,
			.canceled = false,
		};
		uthread->dtv[0] = 0;

		info->uthread = uthread;
	}
	else
	{
		const size_t module_count = self->dtv[0];

		const size_t tls_size = self->master_tls_size
			+ sizeof(uthread)
			+ (module_count + 1) * sizeof(uintptr_t);

		uint8_t* tls_addr = static_cast<uint8_t*>(mmap(nullptr, tls_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
		if (tls_addr == MAP_FAILED)
			goto pthread_create_error;
		memcpy(tls_addr, self->master_tls_addr, self->master_tls_size);

		uthread* uthread = reinterpret_cast<struct uthread*>(tls_addr + self->master_tls_size);
		*uthread = {
			.self = uthread,
			.master_tls_addr = self->master_tls_addr,
			.master_tls_size = self->master_tls_size,
			.cleanup_stack = nullptr,
			.id = -1,
			.errno_ = 0,
			.cancel_type = PTHREAD_CANCEL_DEFERRED,
			.cancel_state = PTHREAD_CANCEL_ENABLE,
			.canceled = 0,
		};

		const uintptr_t self_addr = reinterpret_cast<uintptr_t>(self);
		const uintptr_t uthread_addr = reinterpret_cast<uintptr_t>(uthread);

		uthread->dtv[0] = module_count;
		for (size_t i = 1; i <= module_count; i++)
			uthread->dtv[i] = self->dtv[i] - self_addr + uthread_addr;

		info->uthread = uthread;
	}

	syscall_ret = syscall(SYS_PTHREAD_CREATE, attr, _pthread_trampoline, info);
	if (syscall_ret == -1)
		goto pthread_create_error;

	if (thread_id)
		*thread_id = syscall_ret;
	return 0;

pthread_create_error:
	const int return_code = errno;
	if (info->uthread)
		free_uthread(info->uthread);
	free(info);
	return return_code;
}

int pthread_detach(pthread_t thread)
{
	(void)thread;
	dwarnln("TODO: pthread_detach");
	return ENOTSUP;
}

void pthread_exit(void* value_ptr)
{
	uthread* uthread = _get_uthread();
	while (uthread->cleanup_stack)
		pthread_cleanup_pop(1);

	for (size_t iteration = 0; iteration < PTHREAD_DESTRUCTOR_ITERATIONS; iteration++)
	{
		bool called = false;
		for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
		{
			void (*destructor)(void*) = nullptr;
			void* value = nullptr;

			pthread_spin_lock(&s_pthread_key_lock);
			if (s_pthread_key_map[i] && s_pthread_key_values[i].key == s_pthread_key_map[i])
			{
				destructor = s_pthread_key_destructors[i];
				value = s_pthread_key_values[i].value;
			}
			pthread_spin_unlock(&s_pthread_key_lock);

			if (!value || !destructor)
				continue;
			destructor(value);
			called = true;
		}
		if (!called)
			break;
	}

	free_uthread(uthread);
	syscall(SYS_PTHREAD_EXIT, value_ptr);
	ASSERT_NOT_REACHED();
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
	return t1 == t2;
}

int pthread_join(pthread_t thread, void** value_ptr)
{
	pthread_testcancel();

	errno = 0;
	while (syscall(SYS_PTHREAD_JOIN, thread, value_ptr) == -1 && errno == EINTR)
	{
		pthread_testcancel();
		errno = 0;
	}
	return errno;
}

pthread_t pthread_self(void)
{
	return _get_uthread()->id;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
	static_assert(PTHREAD_ONCE_INIT == 0);

	pthread_once_t expected = 0;
	if (BAN::atomic_compare_exchange(*once_control, expected, 1))
	{
		init_routine();
		BAN::atomic_store(*once_control, 2);
	}

	while (BAN::atomic_load(*once_control) != 2)
		sched_yield();
	return 0;
}

struct pthread_atfork_t
{
	void (*function)();
	pthread_atfork_t* next;
};
static pthread_atfork_t* s_atfork_prepare = nullptr;
static pthread_atfork_t* s_atfork_parent = nullptr;
static pthread_atfork_t* s_atfork_child = nullptr;
static pthread_mutex_t s_atfork_mutex = PTHREAD_MUTEX_INITIALIZER;

void _pthread_call_atfork(int state)
{
	if (state == _PTHREAD_ATFORK_CHILD)
		_get_uthread()->id = syscall(SYS_PTHREAD_SELF);

	pthread_mutex_lock(&s_atfork_mutex);

	pthread_atfork_t* list = nullptr;
	switch (state)
	{
		case _PTHREAD_ATFORK_PREPARE: list = s_atfork_prepare; break;
		case _PTHREAD_ATFORK_PARENT:  list = s_atfork_parent; break;
		case _PTHREAD_ATFORK_CHILD:   list = s_atfork_child; break;
		default:
			ASSERT_NOT_REACHED();
	}

	for (; list; list = list->next)
		list->function();

	pthread_mutex_unlock(&s_atfork_mutex);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void(*child)(void))
{
	pthread_atfork_t* prepare_entry = nullptr;
	if (prepare != nullptr)
		prepare_entry = static_cast<pthread_atfork_t*>(malloc(sizeof(pthread_attr_t)));

	pthread_atfork_t* parent_entry = nullptr;
	if (parent != nullptr)
		parent_entry = static_cast<pthread_atfork_t*>(malloc(sizeof(pthread_attr_t)));

	pthread_atfork_t* child_entry = nullptr;
	if (child != nullptr)
		child_entry = static_cast<pthread_atfork_t*>(malloc(sizeof(pthread_attr_t)));

	if ((prepare && !prepare_entry) || (parent && !parent_entry) || (child && !child_entry))
	{
		if (prepare_entry)
			free(prepare_entry);
		if (parent_entry)
			free(parent_entry);
		if (child_entry)
			free(child_entry);
		return ENOMEM;
	}

	const auto prepend_atfork =
		[](pthread_atfork_t*& list, pthread_atfork_t* entry)
		{
			entry->next = list;
			list = entry;
		};

	const auto append_atfork =
		[](pthread_atfork_t*& list, pthread_atfork_t* entry)
		{
			while (list)
				list = list->next;
			entry->next = nullptr;
			list = entry;
		};

	pthread_mutex_lock(&s_atfork_mutex);

	if (prepare_entry)
	{
		prepare_entry->function = prepare;
		prepend_atfork(s_atfork_prepare, prepare_entry);
	}

	if (parent_entry)
	{
		parent_entry->function = parent;
		append_atfork(s_atfork_parent, parent_entry);
	}

	if (child_entry)
	{
		child_entry->function = parent;
		append_atfork(s_atfork_child, child_entry);
	}

	pthread_mutex_unlock(&s_atfork_mutex);

	return 0;
}

static void pthread_cancel_handler(int)
{
	uthread* uthread = _get_uthread();
	BAN::atomic_store(uthread->canceled, true);
	if (BAN::atomic_load(uthread->cancel_state) != PTHREAD_CANCEL_ENABLE)
		return;
	switch (BAN::atomic_load(uthread->cancel_type))
	{
		case PTHREAD_CANCEL_ASYNCHRONOUS:
			pthread_exit(PTHREAD_CANCELED);
		case PTHREAD_CANCEL_DEFERRED:
			return;
	}
	ASSERT_NOT_REACHED();
}

int pthread_cancel(pthread_t thread)
{
	signal(SIGCANCEL, &pthread_cancel_handler);
	return pthread_kill(thread, SIGCANCEL);
}

int pthread_setcancelstate(int state, int* oldstate)
{
	switch (state)
	{
		case PTHREAD_CANCEL_ENABLE:
		case PTHREAD_CANCEL_DISABLE:
			break;
		default:
			return EINVAL;
	}

	BAN::atomic_exchange(_get_uthread()->cancel_state, state);
	if (oldstate)
		*oldstate = state;
	return 0;
}

int pthread_setcanceltype(int type, int* oldtype)
{
	switch (type)
	{
		case PTHREAD_CANCEL_DEFERRED:
		case PTHREAD_CANCEL_ASYNCHRONOUS:
			break;
		default:
			return EINVAL;
	}

	BAN::atomic_exchange(_get_uthread()->cancel_type, type);
	if (oldtype)
		*oldtype = type;
	return 0;
}

void pthread_testcancel(void)
{
	uthread* uthread = _get_uthread();
	if (BAN::atomic_load(uthread->cancel_state) != PTHREAD_CANCEL_ENABLE)
		return;
	if (!BAN::atomic_load(uthread->canceled))
		return;
	pthread_exit(PTHREAD_CANCELED);
}

int pthread_getschedparam(pthread_t thread, int* __restrict policy, struct sched_param* __restrict param)
{
	(void)thread;
	(void)policy;
	(void)param;
	dwarnln("TODO: pthread_getschedparam");
	return ENOTSUP;
}

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param)
{
	(void)thread;
	(void)policy;
	(void)param;
	dwarnln("TODO: pthread_setschedparam");
	return ENOTSUP;
}

int pthread_spin_destroy(pthread_spinlock_t* lock)
{
	(void)lock;
	return 0;
}

int pthread_spin_init(pthread_spinlock_t* lock, int pshared)
{
	(void)pshared;
	*lock = 0;
	return 0;
}

int pthread_spin_lock(pthread_spinlock_t* lock)
{
	const auto tid = pthread_self();
	ASSERT(BAN::atomic_load(*lock, BAN::MemoryOrder::memory_order_relaxed) != tid);

	pthread_t expected = 0;
	while (!BAN::atomic_compare_exchange(*lock, expected, tid, BAN::MemoryOrder::memory_order_acquire))
	{
		__builtin_ia32_pause();
		expected = 0;
	}

	return 0;
}

int pthread_spin_trylock(pthread_spinlock_t* lock)
{
	const auto tid = pthread_self();
	ASSERT(BAN::atomic_load(*lock, BAN::MemoryOrder::memory_order_relaxed) != tid);

	pthread_t expected = 0;
	if (!BAN::atomic_compare_exchange(*lock, expected, tid, BAN::MemoryOrder::memory_order_acquire))
		return EBUSY;
	return 0;
}

int pthread_spin_unlock(pthread_spinlock_t* lock)
{
	ASSERT(BAN::atomic_load(*lock, BAN::MemoryOrder::memory_order_relaxed) == pthread_self());
	BAN::atomic_store(*lock, 0, BAN::MemoryOrder::memory_order_release);
	return 0;
}

template<typename T>
static int _pthread_timedlock(T* __restrict lock, const struct timespec* __restrict abstime, int (*trylock)(T*))
{
	if (trylock(lock) == 0)
		return 0;

	constexpr auto has_timed_out =
		[](const struct timespec* abstime) -> bool
		{
			struct timespec curtime;
			clock_gettime(CLOCK_REALTIME, &curtime);
			if (curtime.tv_sec < abstime->tv_sec)
				return false;
			if (curtime.tv_sec > abstime->tv_sec)
				return true;
			return curtime.tv_nsec >= abstime->tv_nsec;
		};

	while (!has_timed_out(abstime))
	{
		if (trylock(lock) == 0)
			return 0;
		sched_yield();
	}

	return ETIMEDOUT;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
	*attr = {
		.type = PTHREAD_MUTEX_DEFAULT,
		.shared = false,
	};
	return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* __restrict attr, int* __restrict pshared)
{
	*pshared = attr->shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
	return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared)
{
	switch (pshared)
	{
		case PTHREAD_PROCESS_PRIVATE:
			attr->shared = false;
			return 0;
		case PTHREAD_PROCESS_SHARED:
			attr->shared = true;
			return 0;
	}
	return EINVAL;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict attr, int* __restrict type)
{
	*type = attr->type;
	return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type)
{
	switch (type)
	{
		case PTHREAD_MUTEX_DEFAULT:
		case PTHREAD_MUTEX_ERRORCHECK:
		case PTHREAD_MUTEX_NORMAL:
		case PTHREAD_MUTEX_RECURSIVE:
			attr->type = type;
			return 0;
	}
	return EINVAL;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
	(void)mutex;
	return 0;
}

int pthread_mutex_init(pthread_mutex_t* __restrict mutex, const pthread_mutexattr_t* __restrict attr)
{
	const pthread_mutexattr_t default_attr = {
		.type = PTHREAD_MUTEX_DEFAULT,
		.shared = false,
	};
	if (attr == nullptr)
		attr = &default_attr;
	*mutex = {
		.attr = *attr,
		.locker = 0,
		.lock_depth = 0,
	};
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
	// NOTE: current yielding implementation supports shared

	const auto tid = pthread_self();

	switch (mutex->attr.type)
	{
		case PTHREAD_MUTEX_RECURSIVE:
			if (mutex->locker != tid)
				break;
			mutex->lock_depth++;
			return 0;
		case PTHREAD_MUTEX_ERRORCHECK:
			if (mutex->locker != tid)
				break;
			return EDEADLK;
	}

	pthread_t expected = 0;
	while (!BAN::atomic_compare_exchange(mutex->locker, expected, tid, BAN::MemoryOrder::memory_order_acquire))
	{
		sched_yield();
		expected = 0;
	}

	mutex->lock_depth = 1;
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
	// NOTE: current yielding implementation supports shared

	const auto tid = pthread_self();

	switch (mutex->attr.type)
	{
		case PTHREAD_MUTEX_RECURSIVE:
			if (mutex->locker != tid)
				break;
			mutex->lock_depth++;
			return 0;
		case PTHREAD_MUTEX_ERRORCHECK:
			if (mutex->locker != tid)
				break;
			return EDEADLK;
	}

	pthread_t expected = 0;
	if (!BAN::atomic_compare_exchange(mutex->locker, expected, tid, BAN::MemoryOrder::memory_order_acquire))
		return EBUSY;

	mutex->lock_depth = 1;
	return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime)
{
	return _pthread_timedlock(mutex, abstime, &pthread_mutex_trylock);
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
	// NOTE: current yielding implementation supports shared

	ASSERT(mutex->locker == pthread_self());

	mutex->lock_depth--;
	if (mutex->lock_depth == 0)
		BAN::atomic_store(mutex->locker, 0, BAN::MemoryOrder::memory_order_release);

	return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t* attr)
{
	*attr = {
		.shared = false,
	};
	return 0;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* __restrict attr, int* __restrict pshared)
{
	*pshared = attr->shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
	return 0;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared)
{
	switch (pshared)
	{
		case PTHREAD_PROCESS_PRIVATE:
			attr->shared = false;
			return 0;
		case PTHREAD_PROCESS_SHARED:
			attr->shared = true;
			return 0;
	}
	return EINVAL;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock)
{
	(void)rwlock;
	return 0;
}

int pthread_rwlock_init(pthread_rwlock_t* __restrict rwlock, const pthread_rwlockattr_t* __restrict attr)
{
	const pthread_rwlockattr_t default_attr = {
		.shared = false,
	};
	if (attr == nullptr)
		attr = &default_attr;
	*rwlock = {
		.attr = *attr,
		.lockers = 0,
		.writers = 0,
	};
	return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock)
{
	unsigned expected = BAN::atomic_load(rwlock->lockers);
	for (;;)
	{
		if (expected == rwlock_writer_locked || BAN::atomic_load(rwlock->writers))
			sched_yield();
		else if (BAN::atomic_compare_exchange(rwlock->lockers, expected, expected + 1))
			break;
	}
	return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock)
{
	unsigned expected = BAN::atomic_load(rwlock->lockers);
	while (expected != rwlock_writer_locked && BAN::atomic_load(rwlock->writers) == 0)
		if (BAN::atomic_compare_exchange(rwlock->lockers, expected, expected + 1))
			return 0;
	return EBUSY;
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime)
{
	return _pthread_timedlock(rwlock, abstime, &pthread_rwlock_tryrdlock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock)
{
	BAN::atomic_add_fetch(rwlock->writers, 1);
	unsigned expected = 0;
	while (!BAN::atomic_compare_exchange(rwlock->lockers, expected, rwlock_writer_locked))
	{
		sched_yield();
		expected = 0;
	}
	BAN::atomic_sub_fetch(rwlock->writers, 1);
	return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock)
{
	unsigned expected = 0;
	if (!BAN::atomic_compare_exchange(rwlock->lockers, expected, rwlock_writer_locked))
		return EBUSY;
	return 0;
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime)
{
	return _pthread_timedlock(rwlock, abstime, &pthread_rwlock_trywrlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock)
{
	if (BAN::atomic_load(rwlock->lockers) == rwlock_writer_locked)
		BAN::atomic_store(rwlock->lockers, 0);
	else
		BAN::atomic_sub_fetch(rwlock->lockers, 1);
	return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_condattr_init(pthread_condattr_t* attr)
{
	*attr = {
		.clock = CLOCK_REALTIME,
		.shared = false,
	};
	return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t* __restrict attr, clockid_t* __restrict clock_id)
{
	*clock_id = attr->clock;
	return 0;
}

int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id)
{
	switch (clock_id)
	{
		case CLOCK_MONOTONIC:
		case CLOCK_REALTIME:
			break;
		default:
			return EINVAL;
	}
	attr->clock = clock_id;
	return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t* __restrict attr, int* __restrict pshared)
{
	*pshared = attr->shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
	return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared)
{
	switch (pshared)
	{
		case PTHREAD_PROCESS_PRIVATE:
			attr->shared = false;
			return 0;
		case PTHREAD_PROCESS_SHARED:
			attr->shared = true;
			return 0;
	}
	return EINVAL;
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
	(void)cond;
	return 0;
}

int pthread_cond_init(pthread_cond_t* __restrict cond, const pthread_condattr_t* __restrict attr)
{
	const pthread_condattr_t default_attr = {
		.clock = CLOCK_MONOTONIC,
		.shared = false,
	};
	if (attr == nullptr)
		attr = &default_attr;
	*cond = {
		.attr = *attr,
		.lock = PTHREAD_SPIN_INITIALIZER,
		.block_list = nullptr,
	};
	return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
	pthread_spin_lock(&cond->lock);
	for (auto* block = cond->block_list; block; block = block->next)
		BAN::atomic_store(block->signaled, 1);
	pthread_spin_unlock(&cond->lock);
	return 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
	pthread_spin_lock(&cond->lock);
	if (cond->block_list)
		BAN::atomic_store(cond->block_list->signaled, 1);
	pthread_spin_unlock(&cond->lock);
	return 0;
}

int pthread_cond_wait(pthread_cond_t* __restrict cond, pthread_mutex_t* __restrict mutex)
{
	// pthread_testcancel in pthread_cond_timedwait
	return pthread_cond_timedwait(cond, mutex, nullptr);
}

int pthread_cond_timedwait(pthread_cond_t* __restrict cond, pthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime)
{
	pthread_testcancel();

	constexpr auto has_timed_out =
		[](const struct timespec* abstime, clockid_t clock_id) -> bool
		{
			if (abstime == nullptr)
				return false;
			struct timespec curtime;
			clock_gettime(clock_id, &curtime);
			if (curtime.tv_sec < abstime->tv_sec)
				return false;
			if (curtime.tv_sec > abstime->tv_sec)
				return true;
			return curtime.tv_nsec >= abstime->tv_nsec;
		};

	pthread_spin_lock(&cond->lock);
	_pthread_cond_block block = {
		.next = cond->block_list,
		.signaled = 0,
	};
	cond->block_list = &block;
	pthread_spin_unlock(&cond->lock);

	pthread_mutex_unlock(mutex);

	while (BAN::atomic_load(block.signaled) == 0)
	{
		if (has_timed_out(abstime, cond->attr.clock))
			return ETIMEDOUT;
		sched_yield();
	}

	pthread_spin_lock(&cond->lock);
	if (&block == cond->block_list)
		cond->block_list = block.next;
	else
	{
		_pthread_cond_block* prev = cond->block_list;
		while (prev->next != &block)
			prev = prev->next;
		prev->next = block.next;
	}
	pthread_spin_unlock(&cond->lock);

	pthread_mutex_lock(mutex);
	return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_barrierattr_init(pthread_barrierattr_t* attr)
{
	*attr = {
		.shared = false,
	};
	return 0;
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t* __restrict attr, int* __restrict pshared)
{
	*pshared = attr->shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE;
	return 0;
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared)
{
	switch (pshared)
	{
		case PTHREAD_PROCESS_PRIVATE:
			attr->shared = false;
			return 0;
		case PTHREAD_PROCESS_SHARED:
			attr->shared = true;
			return 0;
	}
	return EINVAL;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier)
{
	(void)barrier;
	return 0;
}

int pthread_barrier_init(pthread_barrier_t* __restrict barrier, const pthread_barrierattr_t* __restrict attr, unsigned count)
{
	if (count == 0)
		return EINVAL;
	const pthread_barrierattr_t default_attr = {
		.shared = false,
	};
	if (attr == nullptr)
		attr = &default_attr;
	*barrier = {
		.attr = *attr,
		.target = count,
		.waiting = 0,
	};
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier)
{
	const unsigned index = BAN::atomic_add_fetch(barrier->waiting, 1);

	// FIXME: this case should be handled, but should be relatively uncommon
	//        so i'll just roll with the easy implementation
	ASSERT(index <= barrier->target);

	if (index == barrier->target)
	{
		BAN::atomic_store(barrier->waiting, 0);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}

	while (BAN::atomic_load(barrier->waiting))
		sched_yield();
	return 0;
}

struct tls_index
{
	unsigned long int ti_module;
	unsigned long int ti_offset;
};

extern "C" void* __tls_get_addr(tls_index* ti)
{
	return reinterpret_cast<void*>(_get_uthread()->dtv[ti->ti_module] + ti->ti_offset);
}

#if ARCH(i686)
extern "C" void* __attribute__((__regparm__(1))) ___tls_get_addr(tls_index* ti)
{
	return reinterpret_cast<void*>(_get_uthread()->dtv[ti->ti_module] + ti->ti_offset);
}
#endif
