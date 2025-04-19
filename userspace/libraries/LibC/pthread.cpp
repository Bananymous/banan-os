#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <BAN/Atomic.h>
#include <BAN/PlacementNew.h>

#include <kernel/Arch.h>

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
	syscall(SYS_SET_TLS, info.uthread);
	free(arg);
	pthread_exit(info.start_routine(info.arg));
	ASSERT_NOT_REACHED();
}

static uthread* get_uthread()
{
	uthread* result;
#if ARCH(x86_64)
	asm volatile("movq %%fs:0, %0" : "=r"(result));
#elif ARCH(i686)
	asm volatile("movl %%gs:0, %0" : "=r"(result));
#endif
	return result;
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

#if not __disable_thread_local_storage
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
#endif

#if not __disable_thread_local_storage
static thread_local struct {
	void* value;
	void (*destructor)(void*);
} s_pthread_keys[PTHREAD_KEYS_MAX] {};
static thread_local uint8_t s_pthread_keys_allocated[(PTHREAD_KEYS_MAX + 7) / 8];

static inline bool is_pthread_key_allocated(pthread_key_t key)
{
	if (key >= PTHREAD_KEYS_MAX)
		return false;
	return s_pthread_keys_allocated[key / 8] & (1 << (key % 8));
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
	for (pthread_key_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (is_pthread_key_allocated(i))
			continue;
		s_pthread_keys[i].value = nullptr;
		s_pthread_keys[i].destructor = destructor;
		s_pthread_keys_allocated[i / 8] |= 1 << (i % 8);
		*key = i;
		return 0;
	}

	return EAGAIN;
}

int pthread_key_delete(pthread_key_t key)
{
	if (!is_pthread_key_allocated(key))
		return EINVAL;
	s_pthread_keys[key].value = nullptr;
	s_pthread_keys[key].destructor = nullptr;
	s_pthread_keys_allocated[key / 8] &= ~(1 << (key % 8));
	return 0;
}

void* pthread_getspecific(pthread_key_t key)
{
	if (!is_pthread_key_allocated(key))
		return nullptr;
	return s_pthread_keys[key].value;
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
	if (!is_pthread_key_allocated(key))
		return EINVAL;
	s_pthread_keys[key].value = const_cast<void*>(value);
	return 0;
}
#endif

int pthread_attr_destroy(pthread_attr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	*attr = 0;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize)
{
	(void)attr;
	(void)stacksize;
	dwarnln("TODO: ignoring pthread_attr_setstacksize");
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

	if (uthread* self = get_uthread(); self->master_tls_addr == nullptr)
	{
		uthread* uthread = static_cast<struct uthread*>(malloc(sizeof(struct uthread) + sizeof(uintptr_t)));
		if (uthread == nullptr)
			goto pthread_create_error;
		uthread->self = uthread;
		uthread->master_tls_addr = nullptr;
		uthread->master_tls_size = 0;
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
		uthread->self = uthread;
		uthread->master_tls_addr = self->master_tls_addr;
		uthread->master_tls_size = self->master_tls_size;

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
#if not __disable_thread_local_storage
	while (s_cleanup_stack)
		pthread_cleanup_pop(1);
	for (size_t iteration = 0; iteration < PTHREAD_DESTRUCTOR_ITERATIONS; iteration++)
	{
		bool called = false;
		for (pthread_key_t i = 0; i < PTHREAD_KEYS_MAX; i++)
		{
			if (!is_pthread_key_allocated(i))
				continue;
			if (!s_pthread_keys[i].value || !s_pthread_keys[i].destructor)
				continue;
			void* old_value = s_pthread_keys[i].value;
			s_pthread_keys[i].value = nullptr;
			s_pthread_keys[i].destructor(old_value);
			called = true;
		}
		if (!called)
			break;
	}
#endif
	free_uthread(get_uthread());
	syscall(SYS_PTHREAD_EXIT, value_ptr);
	ASSERT_NOT_REACHED();
}

int pthread_join(pthread_t thread, void** value_ptr)
{
	return syscall(SYS_PTHREAD_JOIN, thread, value_ptr);
}

pthread_t pthread_self(void)
{
#if __disable_thread_local_storage
	return syscall(SYS_PTHREAD_SELF);
#else
	static thread_local pthread_t s_pthread_self { -1 };
	if (s_pthread_self == -1) [[unlikely]]
		s_pthread_self = syscall(SYS_PTHREAD_SELF);
	return s_pthread_self;
#endif
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

struct tls_index
{
	unsigned long int ti_module;
	unsigned long int ti_offset;
};

extern "C" void* __tls_get_addr(tls_index* ti)
{
	return reinterpret_cast<void*>(get_uthread()->dtv[ti->ti_module] + ti->ti_offset);
}

#if ARCH(i686)
extern "C" void* __attribute__((__regparm__(1))) ___tls_get_addr(tls_index* ti)
{
	return reinterpret_cast<void*>(get_uthread()->dtv[ti->ti_module] + ti->ti_offset);
}
#endif
