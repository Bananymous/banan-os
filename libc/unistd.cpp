#include <BAN/Assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
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

	long ret = -1;

	switch (syscall)
	{
		case SYS_EXIT:
		{
			int exit_code = va_arg(args, int);
			ret = Kernel::syscall(SYS_EXIT, exit_code);
			break;
		}
		case SYS_READ:
		{
			int fd = va_arg(args, int);
			void* buffer = va_arg(args, void*);
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_READ, fd, buffer, bytes);
			break;
		}
		case SYS_WRITE:
		{
			int fd = va_arg(args, int);
			const char* string = va_arg(args, const char*);
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_WRITE, fd, string, bytes);
			break;
		}
		case SYS_TERMID:
		{
			char* buffer = va_arg(args, char*);
			Kernel::syscall(SYS_TERMID, buffer);
			break;
		}
		case SYS_CLOSE:
		{
			int fd = va_arg(args, int);
			ret = Kernel::syscall(SYS_CLOSE, fd);
			break;
		}
		case SYS_SEEK:
		{
			int fd = va_arg(args, int);
			long offset = va_arg(args, long);
			ret = Kernel::syscall(SYS_SEEK, fd, offset);
			break;
		}
		case SYS_OPEN:
		{
			const char* path = va_arg(args, const char*);
			int oflags = va_arg(args, int);
			ret = Kernel::syscall(SYS_OPEN, path, oflags);
			break;
		}
		case SYS_ALLOC:
		{
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_ALLOC, bytes);
			break;
		}
		case SYS_FREE:
		{
			void* ptr = va_arg(args, void*);
			ret = Kernel::syscall(SYS_FREE, ptr);
			break;
		}
		default:
			puts("LibC: Unhandeled syscall");
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
