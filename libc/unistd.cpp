#include <BAN/Assert.h>
#include <errno.h>
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
			size_t offset = va_arg(args, size_t);
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_READ, fd, (uintptr_t)buffer, offset, bytes);
			break;
		}
		case SYS_WRITE:
		{
			int fd = va_arg(args, int);
			const char* string = va_arg(args, const char*);
			size_t offset = va_arg(args, size_t);
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_WRITE, fd, (uintptr_t)string, offset, bytes);
			break;
		}
		case SYS_TERMID:
		{
			char* buffer = va_arg(args, char*);
			ret = Kernel::syscall(SYS_TERMID, (uintptr_t)buffer);
			break;
		}
		case SYS_CLOSE:
		{
			int fd = va_arg(args, int);
			ret = Kernel::syscall(SYS_CLOSE, fd);
			break;
		}
		case SYS_OPEN:
		{
			const char* path = va_arg(args, const char*);
			int oflags = va_arg(args, int);
			ret = Kernel::syscall(SYS_OPEN, (uintptr_t)path, oflags);
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
			ret = Kernel::syscall(SYS_FREE, (uintptr_t)ptr);
			break;
		}
		case SYS_SEEK:
		{
			int fd = va_arg(args, int);
			off_t offset = va_arg(args, off_t);
			int whence = va_arg(args, int);
			ret = Kernel::syscall(SYS_SEEK, fd, offset, whence);
			break;
		}
		case SYS_TELL:
		{
			int fd = va_arg(args, int);
			ret = Kernel::syscall(SYS_TELL, fd);
			break;
		}
		default:
			puts("LibC: Unhandeled syscall");
			ret = -ENOSYS;
			break;
	}

	va_end(args);
	
	if (ret < 0)
	{
		errno = -ret;
		return -1;
	}

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
