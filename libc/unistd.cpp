#include <BAN/Assert.h>
#include <kernel/Syscall.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

char** environ;

extern "C" void _init_stdio();

extern "C" void _init_libc(char** _environ)
{
	environ = _environ;
	_init_stdio();
}

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
		case SYS_GET_TERMIOS:
		{
			struct termios* termios = va_arg(args, struct termios*);
			ret = Kernel::syscall(SYS_GET_TERMIOS, (uintptr_t)termios);
			break;
		}
		case SYS_SET_TERMIOS:
		{
			const struct termios* termios = va_arg(args, const struct termios*);
			ret = Kernel::syscall(SYS_SET_TERMIOS, (uintptr_t)termios);
			break;
		}
		case SYS_FORK:
		{
			ret = Kernel::syscall(SYS_FORK);
			break;
		}
		case SYS_SLEEP:
		{
			unsigned int seconds = va_arg(args, unsigned int);
			ret = Kernel::syscall(SYS_SLEEP, seconds);
			break;
		}
		case SYS_EXEC:
		{
			const char* pathname = va_arg(args, const char*);
			const char* const* argv = va_arg(args, const char* const*);
			const char* const* envp = va_arg(args, const char* const*);
			ret = Kernel::syscall(SYS_EXEC, (uintptr_t)pathname, (uintptr_t)argv, (uintptr_t)envp);
			break;
		}
		case SYS_REALLOC:
		{
			void* ptr = va_arg(args, void*);
			size_t size = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_REALLOC, (uintptr_t)ptr, size);
			break;
		}
		case SYS_WAIT:
		{
			pid_t pid = va_arg(args, pid_t);
			int* stat_loc = va_arg(args, int*);
			int options = va_arg(args, int);
			ret = Kernel::syscall(SYS_WAIT, pid, (uintptr_t)stat_loc, options);
			break;
		}
		case SYS_STAT:
		{
			const char* path = va_arg(args, const char*);
			struct stat* buf = va_arg(args, struct stat*);
			int flags = va_arg(args, int);
			ret = Kernel::syscall(SYS_STAT, (uintptr_t)path, (uintptr_t)buf, flags);
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

int execl(const char* pathname, const char* arg0, ...)
{
	if (arg0 == nullptr)
	{
		char* temp = nullptr;
		return execv(pathname, &temp);
	}
	
	va_list ap;
	va_start(ap, arg0);
	int argc = 1;
	while (va_arg(ap, const char*))
		argc++;
	va_end(ap);

	char** argv = (char**)malloc(sizeof(char*) * (argc + 1));
	if (argv == nullptr)
	{
		errno = ENOMEM;
		return -1;
	}

	va_start(ap, arg0);
	argv[0] = (char*)arg0;
	for (int i = 1; i < argc; i++)
		argv[i] = va_arg(ap, char*);
	argv[argc] = nullptr;
	va_end(ap);

	return execv(pathname, argv);
}

int execle(const char* pathname, const char* arg0, ...)
{	
	va_list ap;

	int argc = 0;

	if (arg0)
	{
		va_start(ap, arg0);
		argc = 1;
		while (va_arg(ap, const char*))
			argc++;
		va_end(ap);
	}

	char** argv = (char**)malloc(sizeof(char*) * (argc + 1));
	if (argv == nullptr)
	{
		errno = ENOMEM;
		return -1;
	}

	char** envp = nullptr;

	va_start(ap, arg0);
	argv[0] = (char*)arg0;
	for (int i = 1; i < argc; i++)
		argv[i] = va_arg(ap, char*);
	argv[argc] = nullptr;
	envp = va_arg(ap, char**);
	va_end(ap);

	return execve(pathname, argv, envp);
}

int execv(const char* pathname, char* const argv[])
{
	return execve(pathname, argv, environ);
}

int execve(const char* pathname, char* const argv[], char* const envp[])
{
	return syscall(SYS_EXEC, pathname, argv, envp);
}

pid_t fork(void)
{
	return syscall(SYS_FORK);
}

unsigned int sleep(unsigned int seconds)
{
	return syscall(SYS_SLEEP, seconds);
}
