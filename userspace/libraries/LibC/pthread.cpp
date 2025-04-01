#include <BAN/Assert.h>

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

static void pthread_trampoline(void* arg)
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

pthread_t pthread_self(void)
{
	return syscall(SYS_PTHREAD_SELF);
}
