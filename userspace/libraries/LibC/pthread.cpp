#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/Debug.h>
#include <BAN/PlacementNew.h>

#include <bits/fork_hook.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

struct pthread_trampoline_info_t
{
	struct uthread* uthread;
	void* (*start_routine)(void*);
	void* arg;
};

static pthread_attr_t s_default_pthread_attr {
	.inheritsched = PTHREAD_INHERIT_SCHED,
	.schedparam   = {},
	.schedpolicy  = SCHED_RR,
	.detachstate  = PTHREAD_CREATE_JOINABLE,
	.scope        = PTHREAD_SCOPE_SYSTEM,
	.stackaddr    = nullptr,
	.stacksize    = 8 * 1024 * 1024,
	.guardsize    = 0,
};

static BAN::Atomic<struct uthread*> s_pending_uthread_deletion { nullptr };

static void _pthread_cancel_handler(int)
{
	uthread* uthread = _get_uthread();
	uthread->canceled = true;
	if (uthread->cancel_state == PTHREAD_CANCEL_DISABLE)
		return;
	if (uthread->cancel_type == PTHREAD_CANCEL_ASYNCHRONOUS)
		pthread_exit(PTHREAD_CANCELED);
}

__attribute__((constructor))
static void _init_pthread()
{
	s_default_pthread_attr.guardsize = getpagesize();

	uthread* uthread = _get_uthread();
	uthread->id              = syscall(SYS_THREAD_GETID);
	uthread->attr            = s_default_pthread_attr;
	uthread->errno_          = 0;
	uthread->libc_owns_stack = false;
	uthread->cancel_type     = PTHREAD_CANCEL_DEFERRED;
	uthread->cancel_state    = PTHREAD_CANCEL_ENABLE;
	uthread->canceled        = false;
	uthread->cleanup_funcs   = nullptr;
	memset(uthread->specific_keys, 0, sizeof(uthread->specific_keys));
	memset(uthread->specific_vals, 0, sizeof(uthread->specific_vals));

	if (auto value = getauxval(AT_STACK_BASE))
		uthread->attr.stackaddr = reinterpret_cast<void*>(value);
	if (auto value = getauxval(AT_STACK_SIZE))
		uthread->attr.stacksize = value;

	char buffer[PATH_MAX];
	const ssize_t link_len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (link_len == -1)
		exit(0xFF);
	buffer[link_len] = '\0';

	const char* last_slash = strrchr(buffer, '/');
	if (last_slash == nullptr)
		last_slash = buffer - 1;

	strncpy(uthread->name, last_slash + 1, sizeof(uthread->name));
	uthread->name[sizeof(uthread->name) - 1] = '\0';

	signal(SIGCANCEL, &_pthread_cancel_handler);
}

// stack is 16 byte aligned on entry, this `call` is used to align it
extern "C" void _pthread_trampoline(void*);
asm(
#if defined(__x86_64__)
"_pthread_trampoline:"
	"popq %rdi;"
	"andq $-16, %rsp;"
	"xorq %rbp, %rbp;"
	"call _pthread_trampoline_cpp"
#elif defined(__i686__)
"_pthread_trampoline:"
	"popl %edi;"
	"andl $-16, %esp;"
	"xorl %ebp, %ebp;"
	"subl $12, %esp;"
	"pushl %edi;"
	"call _pthread_trampoline_cpp@plt"
#endif
);

extern "C" void _pthread_trampoline_cpp(void* arg)
{
	auto info = *static_cast<pthread_trampoline_info_t*>(arg);

	BAN::atomic_store(info.uthread->id, syscall(SYS_THREAD_GETID));

#if defined(__x86_64__)
	syscall(SYS_SET_FSBASE, info.uthread);
#elif defined(__i686__)
	syscall(SYS_SET_GSBASE, info.uthread);
#else
#error
#endif

	// NOTE: we have to get id and set TLS to for free to be able to use pthread_self
	free(arg);

	signal(SIGCANCEL, &_pthread_cancel_handler);
	if (info.uthread->attr.detachstate == PTHREAD_CREATE_DETACHED)
		pthread_detach(info.uthread);

	pthread_exit(info.start_routine(info.arg));
	ASSERT_NOT_REACHED();
}

static void free_uthread(uthread* uthread)
{
	for (size_t i = uthread->master_tls_module_count + 1; i <= uthread->dtv[0]; i++)
	{
		if (uthread->dtv[i] == 0)
			continue;
		munmap(
			reinterpret_cast<void*>(uthread->dtv[i]),
			uthread->dynamic_tls->entries[i].master_size
		);
	}

	if (uthread->libc_owns_stack)
		munmap(static_cast<uint8_t*>(uthread->attr.stackaddr) - uthread->attr.guardsize, uthread->attr.stacksize + uthread->attr.guardsize);

	uint8_t* tls_addr = reinterpret_cast<uint8_t*>(uthread) - uthread->master_tls_size;
	const size_t tls_size = uthread->master_tls_size + sizeof(struct uthread);
	munmap(tls_addr, tls_size);
}

void pthread_cleanup_pop(int execute)
{
	uthread* uthread = _get_uthread();
	ASSERT(uthread->cleanup_funcs);

	auto* cleanup = uthread->cleanup_funcs;
	uthread->cleanup_funcs = cleanup->next;

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
	cleanup->next = uthread->cleanup_funcs;

	uthread->cleanup_funcs = cleanup;
}

static pthread_key_t s_pthread_key_current = 1;
static pthread_key_t s_pthread_key_map[PTHREAD_KEYS_MAX] {};
static void (*s_pthread_key_destructors[PTHREAD_KEYS_MAX])(void*) {};
FORK_SAFE_MUTEX(0, s_pthread_key_mutex, PTHREAD_MUTEX_INITIALIZER);

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
	int ret = EAGAIN;

	pthread_mutex_lock(&s_pthread_key_mutex);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i])
			continue;
		s_pthread_key_destructors[i] = destructor;
		s_pthread_key_map[i] = *key = s_pthread_key_current++;
		ret = 0;
		break;
	}
	pthread_mutex_unlock(&s_pthread_key_mutex);

	return ret;
}

int pthread_key_delete(pthread_key_t key)
{
	int ret = EINVAL;

	pthread_mutex_lock(&s_pthread_key_mutex);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		s_pthread_key_destructors[i] = nullptr;
		s_pthread_key_map[i] = 0;
		ret = 0;
		break;
	}
	pthread_mutex_unlock(&s_pthread_key_mutex);

	return ret;
}

void* pthread_getspecific(pthread_key_t key)
{
	void* ret = nullptr;

	auto* uthread = _get_uthread();

	pthread_mutex_lock(&s_pthread_key_mutex);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		if (uthread->specific_keys[i] != key)
		{
			uthread->specific_keys[i] = key;
			uthread->specific_vals[i] = nullptr;
		}
		ret = uthread->specific_vals[i];
		break;
	}
	pthread_mutex_unlock(&s_pthread_key_mutex);

	return ret;
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
	int ret = EINVAL;

	auto* uthread = _get_uthread();

	pthread_mutex_lock(&s_pthread_key_mutex);
	for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
	{
		if (s_pthread_key_map[i] != key)
			continue;
		if (uthread->specific_keys[i] != key)
			uthread->specific_keys[i] = key;
		uthread->specific_vals[i] = const_cast<void*>(value);
		ret = 0;
		break;
	}
	pthread_mutex_unlock(&s_pthread_key_mutex);

	return ret;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
	(void)attr;
	return 0;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	*attr = s_default_pthread_attr;
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
	if (auto rem = guardsize % getpagesize())
		guardsize += getpagesize() - rem;
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
	*stackaddr = attr->stackaddr;
	*stacksize = attr->stacksize;
	return 0;
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize)
{
	if (stacksize < PTHREAD_STACK_MIN)
		return EINVAL;
	if (reinterpret_cast<uintptr_t>(stackaddr) % getpagesize())
		return EINVAL;
	if (stacksize % getpagesize())
		return EINVAL;
	attr->stackaddr = stackaddr;
	attr->stacksize = stacksize;
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t* __restrict attr, size_t* __restrict stacksize)
{
	*stacksize = attr->stacksize;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize)
{
	if (stacksize < PTHREAD_STACK_MIN)
		return EINVAL;
	if (auto rem = stacksize % getpagesize())
		stacksize += getpagesize() - rem;
	attr->stackaddr = nullptr;
	attr->stacksize = stacksize;
	return 0;
}

int pthread_create(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr, void* (*start_routine)(void*), void* __restrict arg)
{
	auto* info = static_cast<pthread_trampoline_info_t*>(malloc(sizeof(pthread_trampoline_info_t)));
	if (info == nullptr)
		return errno;

	*info = {
		.uthread = nullptr,
		.start_routine = start_routine,
		.arg = arg,
	};

	uthread* result = nullptr;

	{
		uthread* self = _get_uthread();

		const size_t tls_size = self->master_tls_size + sizeof(uthread);

		uint8_t* tls_addr = static_cast<uint8_t*>(mmap(nullptr, tls_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
		if (tls_addr == MAP_FAILED)
			goto pthread_create_error;

		uthread* uthread = reinterpret_cast<struct uthread*>(tls_addr + self->master_tls_size);
		*uthread = {
			.self = uthread,
			.master_tls_addr = self->master_tls_addr,
			.master_tls_size = self->master_tls_size,
			.master_tls_module_count = self->master_tls_module_count,
			.dynamic_tls = self->dynamic_tls,
			.id = -1,
			.attr = attr ? *attr : s_default_pthread_attr,
			.name = {},
			.errno_ = 0,
			.libc_owns_stack = false,
			.has_exited = false,
			.cancel_type = PTHREAD_CANCEL_DEFERRED,
			.cancel_state = PTHREAD_CANCEL_ENABLE,
			.canceled = 0,
			.cleanup_funcs = nullptr,
			.specific_keys = {},
			.specific_vals = {},
			.dtv = { self->dtv[0] }
		};
		strcpy(uthread->name, self->name);

		if (self->master_tls_addr && self->master_tls_size)
			memcpy(tls_addr, self->master_tls_addr, self->master_tls_size);

		const ptrdiff_t dtv_offset = reinterpret_cast<uint8_t*>(uthread) - reinterpret_cast<uint8_t*>(self);
		for (size_t i = 1; i <= self->master_tls_module_count; i++)
			uthread->dtv[i] = self->dtv[i] + dtv_offset;

		info->uthread = uthread;

		if (uthread->attr.stackaddr == nullptr)
		{
			ASSERT(uthread->attr.stacksize % getpagesize() == 0);
			ASSERT(uthread->attr.guardsize % getpagesize() == 0);

			void* stack_addr = mmap(nullptr, uthread->attr.stacksize + uthread->attr.guardsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (stack_addr == nullptr)
				goto pthread_create_error;
			uthread->attr.stackaddr = static_cast<uint8_t*>(stack_addr) + uthread->attr.guardsize;
			uthread->libc_owns_stack = true;

			if (uthread->attr.guardsize != 0 && mprotect(stack_addr, uthread->attr.guardsize, PROT_NONE) == -1)
				goto pthread_create_error;
		}

		result = uthread;
	}

	if (syscall(SYS_THREAD_CREATE, _pthread_trampoline, info, info->uthread->attr.stackaddr, info->uthread->attr.stacksize) == -1)
		goto pthread_create_error;

	// wait for the thread to initialize its id
	while (BAN::atomic_load(result->id) == -1)
		sched_yield();

	if (thread)
		*thread = result;
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
	// FIXME: race if detaching while the thread exits

	if (syscall(SYS_THREAD_DETACH, thread->id) == -1)
		return errno;

	thread->attr.detachstate = PTHREAD_CREATE_DETACHED;
	if (thread->has_exited)
		free_uthread(thread);

	return 0;
}

void pthread_exit(void* value_ptr)
{
	uthread* uthread = _get_uthread();
	while (uthread->cleanup_funcs)
		pthread_cleanup_pop(1);

	for (size_t iteration = 0; iteration < PTHREAD_DESTRUCTOR_ITERATIONS; iteration++)
	{
		bool called = false;
		for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++)
		{
			void (*destructor)(void*) = nullptr;
			void* value = nullptr;

			pthread_mutex_lock(&s_pthread_key_mutex);
			if (s_pthread_key_map[i] && uthread->specific_keys[i] == s_pthread_key_map[i])
			{
				destructor = s_pthread_key_destructors[i];
				value = uthread->specific_vals[i];
				uthread->specific_vals[i] = nullptr;
			}
			pthread_mutex_unlock(&s_pthread_key_mutex);

			if (!value || !destructor)
				continue;
			destructor(value);
			called = true;
		}
		if (!called)
			break;
	}

	uthread->has_exited = true;

	if (uthread->attr.detachstate == PTHREAD_CREATE_DETACHED)
	{
		if (uthread->libc_owns_stack)
		{
			if (auto* pending = s_pending_uthread_deletion.exchange(uthread))
			{
				while (BAN::atomic_load(pending->id) != -1)
					sched_yield();
				free_uthread(pending);
			}
			// NOTE: after this store, our stack may get deleted but it should be *fine*
			BAN::atomic_store(uthread->id, -1);
			_kas_syscall(SYS_THREAD_EXIT, nullptr);
			__builtin_trap();
		}

		free_uthread(uthread);
	}

	syscall(SYS_THREAD_EXIT, value_ptr);

	ASSERT_NOT_REACHED();
}

int pthread_join(pthread_t thread, void** value_ptr)
{
	if (thread->attr.detachstate != PTHREAD_CREATE_JOINABLE)
		return EINVAL;

	do {
		pthread_testcancel();
		errno = 0;
	} while (syscall(SYS_THREAD_JOIN, thread->id, value_ptr) == -1 && errno == EINTR);

	const int ret = errno;
	if (ret == 0)
		free_uthread(thread);
	return ret;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
	static_assert(PTHREAD_ONCE_INIT == 0);

	pthread_once_t expected = 0;
	if (BAN::atomic_compare_exchange(*once_control, expected, 1))
	{
		init_routine();
		BAN::atomic_store(*once_control, 2);
		futex(FUTEX_WAKE_PRIVATE, once_control, -1, nullptr);
	}
	else
	{
		while (BAN::atomic_load(*once_control) == 1)
			futex(FUTEX_WAIT_PRIVATE, once_control, 1, nullptr);
	}

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

static size_t s_fork_hook_count;
static fork_hook_t s_fork_hooks[16];

__attribute__((constructor))
static void fork_hook_initializer()
{
	extern const fork_hook_t __start_fork_hooks[];
	extern const fork_hook_t __stop_fork_hooks[];

	s_fork_hook_count = __stop_fork_hooks - __start_fork_hooks;
	ASSERT(s_fork_hook_count < sizeof(s_fork_hooks) / sizeof(fork_hook_t));

	memcpy(s_fork_hooks, __start_fork_hooks, s_fork_hook_count * sizeof(fork_hook_t));
	qsort_r(s_fork_hooks, s_fork_hook_count, sizeof(fork_hook_t), [](const void* a, const void* b, void*) -> int {
		return static_cast<const fork_hook_t*>(a)->priority - static_cast<const fork_hook_t*>(b)->priority;
	}, nullptr);
}

pid_t _pthread_fork()
{
	pthread_mutex_lock(&s_atfork_mutex);

	for (auto* list = s_atfork_prepare; list; list = list->next)
		list->function();

	for (size_t i = 0; i < s_fork_hook_count; i++)
		s_fork_hooks[i].prepare();

	const pid_t pid = syscall(SYS_FORK);

	if (pid == 0)
	{
		_get_uthread()->id = syscall(SYS_THREAD_GETID);

		for (size_t i = 0; i < s_fork_hook_count; i++)
			s_fork_hooks[s_fork_hook_count - i - 1].child();

		for (auto* list = s_atfork_child; list; list = list->next)
			list->function();

		s_atfork_mutex = PTHREAD_MUTEX_INITIALIZER;
	}
	else
	{
		for (size_t i = 0; i < s_fork_hook_count; i++)
			s_fork_hooks[s_fork_hook_count - i - 1].parent();

		for (auto* list = s_atfork_parent; list; list = list->next)
			list->function();

		pthread_mutex_unlock(&s_atfork_mutex);
	}

	return pid;
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

int pthread_cancel(pthread_t thread)
{
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

int pthread_getschedparam(pthread_t thread, int* __restrict policy, struct sched_param* __restrict param)
{
	*policy = thread->attr.schedpolicy;
	*param  = thread->attr.schedparam;
	return 0;
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
	pthread_mutexattr_t default_attr;
	pthread_mutexattr_init(&default_attr);
	if (attr == nullptr)
		attr = &default_attr;

	*mutex = {
		.attr = *attr,
		.futex = 0,
		.waiters = 0,
		.lock_depth = 0,
	};

	return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
	return pthread_mutex_timedlock(mutex, nullptr);
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
	const uint32_t tid = pthread_self()->id;

	switch (mutex->attr.type)
	{
		case PTHREAD_MUTEX_RECURSIVE:
			if (BAN::atomic_load(mutex->futex) != tid)
				break;
			mutex->lock_depth++;
			return 0;
		case PTHREAD_MUTEX_ERRORCHECK:
			if (BAN::atomic_load(mutex->futex) != tid)
				break;
			return EDEADLK;
	}

	uint32_t expected = 0;
	if (!BAN::atomic_compare_exchange(mutex->futex, expected, tid, BAN::MemoryOrder::memory_order_acquire))
		return EBUSY;

	mutex->lock_depth = 1;
	return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime)
{
	// recursive/errorcheck handled in trylock to remove code duplication
	if (const int ret = pthread_mutex_trylock(mutex); ret != EBUSY)
		return ret;

	const uint32_t tid = pthread_self()->id;

	uint32_t expected = 0;
	while (!BAN::atomic_compare_exchange(mutex->futex, expected, tid, BAN::memory_order_acquire))
	{
		const int op = FUTEX_WAIT | (mutex->attr.shared ? 0 : FUTEX_PRIVATE) | FUTEX_REALTIME;

		BAN::atomic_add_fetch(mutex->waiters, 1);
		const auto err = futex(op, &mutex->futex, expected, abstime);
		BAN::atomic_sub_fetch(mutex->waiters, 1);

		if (err && err != EINTR && err != EAGAIN)
			return err;

		expected = 0;
	}

	mutex->lock_depth = 1;
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
	// NOTE: UB for DEFAULT and NORMAL
	if (mutex->futex != static_cast<uint32_t>(pthread_self()->id))
		return EPERM;

	mutex->lock_depth--;
	if (mutex->lock_depth == 0)
	{
		const int op = FUTEX_WAKE | (mutex->attr.shared ? 0 : FUTEX_PRIVATE);

		BAN::atomic_store(mutex->futex, 0);
		if (BAN::atomic_load(mutex->waiters))
			futex(op, &mutex->futex, 1, nullptr);
	}

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
	pthread_mutex_destroy(&rwlock->lock);
	pthread_cond_destroy(&rwlock->cond);
	return 0;
}

int pthread_rwlock_init(pthread_rwlock_t* __restrict rwlock, const pthread_rwlockattr_t* __restrict attr)
{
	pthread_rwlockattr_t default_attr;
	pthread_rwlockattr_init(&default_attr);
	if (attr == nullptr)
		attr = &default_attr;

	*rwlock = {
		.lock = {},
		.cond = {},
		.writer_active = 0,
		.readers_active = 0,
	};

	const pthread_mutexattr_t mattr {
		.type = PTHREAD_MUTEX_DEFAULT,
		.shared = attr->shared,
	};
	pthread_mutex_init(&rwlock->lock, &mattr);

	const pthread_condattr_t cattr {
		.clock = CLOCK_REALTIME,
		.shared = attr->shared,
	};
	pthread_cond_init(&rwlock->cond, &cattr);

	return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock)
{
	return pthread_rwlock_timedrdlock(rwlock, nullptr);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock)
{
	int ret = 0;
	pthread_mutex_lock(&rwlock->lock);
	if (!rwlock->writer_active)
		rwlock->readers_active++;
	else
		ret = EBUSY;
	pthread_mutex_unlock(&rwlock->lock);
	return ret;
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime)
{
	int ret = 0;
	pthread_mutex_lock(&rwlock->lock);
	while (ret == 0 && rwlock->writer_active)
		ret = pthread_cond_timedwait(&rwlock->cond, &rwlock->lock, abstime);
	if (ret == 0)
		rwlock->readers_active++;
	pthread_mutex_unlock(&rwlock->lock);
	return ret;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock)
{
	return pthread_rwlock_timedwrlock(rwlock, nullptr);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock)
{
	int ret = 0;
	pthread_mutex_lock(&rwlock->lock);
	if (!rwlock->readers_active && !rwlock->writer_active)
		rwlock->writer_active = 1;
	else
		ret = EBUSY;
	pthread_mutex_unlock(&rwlock->lock);
	return ret;
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime)
{
	int ret = 0;
	pthread_mutex_lock(&rwlock->lock);
	while (ret == 0 && (rwlock->readers_active || rwlock->writer_active))
		ret = pthread_cond_timedwait(&rwlock->cond, &rwlock->lock, abstime);
	if (ret == 0)
		rwlock->writer_active = 1;
	pthread_mutex_unlock(&rwlock->lock);
	return ret;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock)
{
	pthread_mutex_lock(&rwlock->lock);
	if (rwlock->writer_active)
	{
		rwlock->writer_active = 0;
		pthread_cond_broadcast(&rwlock->cond);
	}
	else
	{
		ASSERT(rwlock->readers_active > 0);
		rwlock->readers_active--;
		if (rwlock->readers_active == 0)
			pthread_cond_broadcast(&rwlock->cond);
	}
	pthread_mutex_unlock(&rwlock->lock);
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
	pthread_condattr_t default_attr;
	pthread_condattr_init(&default_attr);
	if (attr == nullptr)
		attr = &default_attr;

	*cond = {
		.attr = *attr,
		.futex = 0,
	};

	return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
	BAN::atomic_fetch_add(cond->futex, 1);
	const int op = FUTEX_WAKE | (cond->attr.shared ? 0 : FUTEX_PRIVATE);
	futex(op, &cond->futex, -1, nullptr);
	return 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
	BAN::atomic_fetch_add(cond->futex, 1);
	const int op = FUTEX_WAKE | (cond->attr.shared ? 0 : FUTEX_PRIVATE);
	futex(op, &cond->futex, 1, nullptr);
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

	const uint32_t seq = BAN::atomic_load(cond->futex);

	pthread_mutex_unlock(mutex);

	int ret = 0;
	while (ret == 0 && BAN::atomic_load(cond->futex) == seq)
	{
		const int op = FUTEX_WAIT
			| (cond->attr.shared ? 0 : FUTEX_PRIVATE)
			| (cond->attr.clock == CLOCK_REALTIME ? FUTEX_REALTIME : 0);
		const int err = futex(op, &cond->futex, seq, abstime);

		// TODO: what if we get canceled before futex()
		if (const auto* uthread = pthread_self(); uthread->cancel_state == PTHREAD_CANCEL_ENABLE && uthread->canceled)
		{
			pthread_mutex_lock(mutex);
			pthread_exit(PTHREAD_CANCELED);
		}

		if (err && err != EINTR && err != EAGAIN)
			ret = err;
	}

	pthread_mutex_lock(mutex);

	return ret;
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
	pthread_mutex_destroy(&barrier->lock);
	pthread_cond_destroy(&barrier->cond);
	return 0;
}

int pthread_barrier_init(pthread_barrier_t* __restrict barrier, const pthread_barrierattr_t* __restrict attr, unsigned count)
{
	if (count == 0)
		return EINVAL;

	pthread_barrierattr_t default_attr;
	pthread_barrierattr_init(&default_attr);
	if (attr == nullptr)
		attr = &default_attr;

	*barrier = {
		.lock = {},
		.cond = {},
		.target = count,
		.waiting = 0,
		.generation = 0,
	};

	const pthread_mutexattr_t mattr {
		.type = PTHREAD_MUTEX_DEFAULT,
		.shared = attr->shared,
	};
	pthread_mutex_init(&barrier->lock, &mattr);

	const pthread_condattr_t cattr {
		.clock = CLOCK_REALTIME,
		.shared = attr->shared,
	};
	pthread_cond_init(&barrier->cond, &cattr);

	return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier)
{
	pthread_mutex_lock(&barrier->lock);

	const auto gen = barrier->generation;
	barrier->waiting++;

	if (barrier->waiting == barrier->target)
	{
		barrier->waiting = 0;
		barrier->generation++;
		pthread_cond_broadcast(&barrier->cond);
		pthread_mutex_unlock(&barrier->lock);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}

	while (barrier->generation == gen)
		pthread_cond_wait(&barrier->cond, &barrier->lock);
	pthread_mutex_unlock(&barrier->lock);
	return 0;
}

int pthread_getattr_np(pthread_t thread, pthread_attr_t* attr)
{
	*attr = thread->attr;
	return 0;
}

int pthread_getattr_default_np(pthread_attr_t* attr)
{
	*attr = s_default_pthread_attr;
	return 0;
}

int pthread_setattr_default_np(const pthread_attr_t* attr)
{
	// TODO: validate, make thread safe?
	s_default_pthread_attr = *attr;
	return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t size)
{
	const size_t namelen = strlen(thread->name);
	if (size < namelen + 1)
		return ERANGE;
	strcpy(name, thread->name);
	return 0;
}

int pthread_setname_np(pthread_t thread, const char* name)
{
	const size_t namelen = strlen(name);
	if (namelen >= sizeof(thread->name))
		return ERANGE;
	strcpy(thread->name, name);
	return 0;
}

static void load_dynamic_tls_module(size_t module)
{
	auto* uthread = _get_uthread();
	ASSERT(uthread->dynamic_tls);
	ASSERT(module > uthread->master_tls_module_count);

	const _dynamic_tls_entry_t entry = ({
		int expected = 0;
		while (BAN::atomic_compare_exchange(uthread->dynamic_tls->lock, expected, 1))
		{
			sched_yield();
			expected = 0;
		}

		ASSERT(module <= uthread->master_tls_module_count + uthread->dynamic_tls->entry_count);
		auto result = uthread->dynamic_tls->entries[module - uthread->master_tls_module_count - 1];

		BAN::atomic_store(uthread->dynamic_tls->lock, 0);

		result;
	});

	void* dtv_data = mmap(nullptr, entry.master_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT(dtv_data != MAP_FAILED);

	memcpy(dtv_data, entry.master_addr, entry.master_size);

	if (module > uthread->dtv[0])
		uthread->dtv[0]= module;
	uthread->dtv[module] = reinterpret_cast<uintptr_t>(dtv_data);
}

struct tls_index
{
	unsigned long int ti_module;
	unsigned long int ti_offset;
};

extern "C" void* __tls_get_addr(tls_index* ti)
{
	auto* uthread = _get_uthread();
	if (uthread->dtv[ti->ti_module] == 0) [[unlikely]]
		load_dynamic_tls_module(ti->ti_module);
	return reinterpret_cast<void*>(uthread->dtv[ti->ti_module] + ti->ti_offset);
}

#if defined(__i686__)
extern "C" void* __attribute__((__regparm__(1))) ___tls_get_addr(tls_index* ti)
{
	auto* uthread = _get_uthread();
	if (uthread->dtv[ti->ti_module] == 0) [[unlikely]]
		load_dynamic_tls_module(ti->ti_module);
	return reinterpret_cast<void*>(uthread->dtv[ti->ti_module] + ti->ti_offset);
}
#endif

#undef pthread_equal
int pthread_equal(pthread_t t1, pthread_t t2)
{
	return _pthread_equal(t1, t2);
}

#undef pthread_self
pthread_t pthread_self(void)
{
	return _pthread_self();
}

#undef pthread_testcancel
void pthread_testcancel(void)
{
	_pthread_testcancel();
}
