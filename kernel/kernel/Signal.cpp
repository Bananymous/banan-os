#include <sys/syscall.h>
#include <kernel/Syscall.h>

extern "C" __attribute__((section(".userspace")))
void signal_done(int signal)
{
	Kernel::syscall(SYS_SIGNAL_DONE, signal);
}
