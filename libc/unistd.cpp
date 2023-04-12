#include <BAN/Assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>

void _exit(int status)
{
	syscall(SYS_EXIT, status);
	ASSERT_NOT_REACHED();
}

long syscall(long syscall, ...)
{
	va_list args;
	va_start(args, syscall);

	long ret = 0;

	switch (syscall)
	{
	case SYS_EXIT:
		ret = Kernel::syscall(SYS_EXIT, va_arg(args, int));
		break;
	case SYS_READ:
		ret = Kernel::syscall(SYS_READ, va_arg(args, int), va_arg(args, void*), va_arg(args, size_t));
		break;
	case SYS_WRITE:
		ret = Kernel::syscall(SYS_WRITE, va_arg(args, int), va_arg(args, const void*), va_arg(args, size_t));
		break;
	}

	va_end(args);
	
	return ret;
}

pid_t fork(void)
{
	return -1;
}

int execv(const char*, char* const[])
{
	return -1;
}

int execve(const char*, char* const[], char* const[])
{
	return -1;
}

int execvp(const char*, char* const[])
{
	return -1;
}

pid_t getpid(void)
{
	return -1;
}
