#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <kernel/API/SharedPage.h>

extern volatile Kernel::API::SharedPage* g_shared_page;

int sched_get_priority_max(int policy)
{
	(void)policy;
	return 0;
}

int sched_get_priority_min(int policy)
{
	(void)policy;
	return 0;
}

int sched_yield(void)
{
	return syscall(SYS_YIELD);
}

int sched_getcpu(void)
{
	if (g_shared_page == nullptr)
		return -1;

	uint8_t cpu;
#if defined(__x86_64__)
	asm volatile("movb %%gs:0, %0" : "=r"(cpu));
#elif defined(__i686__)
	asm volatile("movb %%fs:0, %0" : "=q"(cpu));
#endif
	return cpu;
}
