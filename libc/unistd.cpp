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

extern "C" void _init_libc(char** _environ)
{
	environ = _environ;
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
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_READ, fd, (uintptr_t)buffer, bytes);
			break;
		}
		case SYS_WRITE:
		{
			int fd = va_arg(args, int);
			const void* buffer = va_arg(args, const void*);
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_WRITE, fd, (uintptr_t)buffer, bytes);
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
		case SYS_OPENAT:
		{
			int fd = va_arg(args, int);
			const char* path = va_arg(args, const char*);
			int oflags = va_arg(args, int);
			ret = Kernel::syscall(SYS_OPENAT, fd, (uintptr_t)path, oflags);
			break;
		}
		case SYS_ALLOC:
		{
			size_t bytes = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_ALLOC, bytes);
			break;
		}
		case SYS_REALLOC:
		{
			void* ptr = va_arg(args, void*);
			size_t size = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_REALLOC, (uintptr_t)ptr, size);
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
		case SYS_EXEC:
		{
			const char* pathname = va_arg(args, const char*);
			const char* const* argv = va_arg(args, const char* const*);
			const char* const* envp = va_arg(args, const char* const*);
			ret = Kernel::syscall(SYS_EXEC, (uintptr_t)pathname, (uintptr_t)argv, (uintptr_t)envp);
			break;
		}
		case SYS_SLEEP:
		{
			unsigned int seconds = va_arg(args, unsigned int);
			ret = Kernel::syscall(SYS_SLEEP, seconds);
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
		case SYS_FSTAT:
		{
			int fd = va_arg(args, int);
			struct stat* buf = va_arg(args, struct stat*);
			ret = Kernel::syscall(SYS_FSTAT, (uintptr_t)fd, (uintptr_t)buf);
			break;
		}
		case SYS_SETENVP:
		{
			char** envp = va_arg(args, char**);
			ret = Kernel::syscall(SYS_SETENVP, (uintptr_t)envp);
			break;
		}
		case SYS_READ_DIR_ENTRIES:
		{
			int fd = va_arg(args, int);
			void* buffer = va_arg(args, void*);
			size_t buffer_size = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_READ_DIR_ENTRIES, fd, (uintptr_t)buffer, buffer_size);
			break;
		}
		case SYS_SET_UID:
		{
			uid_t uid = va_arg(args, uid_t);
			ret = Kernel::syscall(SYS_SET_UID, uid);
			break;
		}
		case SYS_SET_GID:
		{
			gid_t gid = va_arg(args, gid_t);
			ret = Kernel::syscall(SYS_SET_GID, gid);
			break;
		}
		case SYS_SET_EUID:
		{
			uid_t uid = va_arg(args, uid_t);
			ret = Kernel::syscall(SYS_SET_EUID, uid);
			break;
		}
		case SYS_SET_EGID:
		{
			gid_t gid = va_arg(args, gid_t);
			ret = Kernel::syscall(SYS_SET_EGID, gid);
			break;
		}
		case SYS_SET_REUID:
		{
			uid_t ruid = va_arg(args, uid_t);
			uid_t euid = va_arg(args, uid_t);
			ret = Kernel::syscall(SYS_SET_REUID, ruid, euid);
			break;
		}
		case SYS_SET_REGID:
		{
			gid_t rgid = va_arg(args, gid_t);
			gid_t egid = va_arg(args, gid_t);
			ret = Kernel::syscall(SYS_SET_REGID, rgid, egid);
			break;
		}
		case SYS_GET_UID:
		{
			ret = Kernel::syscall(SYS_GET_UID);
			break;
		}
		case SYS_GET_GID:
		{
			ret = Kernel::syscall(SYS_GET_GID);
			break;
		}
		case SYS_GET_EUID:
		{
			ret = Kernel::syscall(SYS_GET_EUID);
			break;
		}
		case SYS_GET_EGID:
		{
			ret = Kernel::syscall(SYS_GET_EGID);
			break;
		}
		case SYS_GET_PWD:
		{
			char* buffer = va_arg(args, char*);
			size_t size = va_arg(args, size_t);
			ret = Kernel::syscall(SYS_GET_PWD, (uintptr_t)buffer, size);
			break;
		}
		case SYS_SET_PWD:
		{
			const char* path = va_arg(args, const char*);
			ret = Kernel::syscall(SYS_SET_PWD, (uintptr_t)path);
			break;
		}
		case SYS_CLOCK_GETTIME:
		{
			clockid_t clock_id = va_arg(args, clockid_t);
			timespec* tp = va_arg(args, timespec*);
			ret = Kernel::syscall(SYS_CLOCK_GETTIME, clock_id, (uintptr_t)tp);
			break;
		}
		case SYS_PIPE:
		{
			int* fildes = va_arg(args, int*);
			ret = Kernel::syscall(SYS_PIPE, (uintptr_t)fildes);
			break;
		}
		case SYS_DUP2:
		{
			int fildes = va_arg(args, int);
			int fildes2 = va_arg(args, int);
			ret = Kernel::syscall(SYS_DUP2, fildes, fildes2);
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

int close(int fd)
{
	return syscall(SYS_CLOSE, fd);
}

ssize_t read(int fildes, void* buf, size_t nbyte)
{
	return syscall(SYS_READ, fildes, buf, nbyte);
}

ssize_t write(int fildes, const void* buf, size_t nbyte)
{
	return syscall(SYS_WRITE, fildes, buf, nbyte);
}

int dup2(int fildes, int fildes2)
{
	return syscall(SYS_DUP2, fildes, fildes2);
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

int pipe(int fildes[2])
{
	return syscall(SYS_PIPE, fildes);
}

unsigned int sleep(unsigned int seconds)
{
	return syscall(SYS_SLEEP, seconds);
}

char* getcwd(char* buf, size_t size)
{
	if (size == 0)
	{
		errno = EINVAL;
		return nullptr;
	}

	if ((char*)syscall(SYS_GET_PWD, buf, size) == nullptr)
		return nullptr;

	setenv("PWD", buf, 1);
	return buf;
}

int chdir(const char* path)
{
	return syscall(SYS_SET_PWD, path);
}

uid_t getuid(void)
{
	return syscall(SYS_GET_UID);
}

gid_t getgid(void)
{
	return syscall(SYS_GET_GID);
}

uid_t geteuid(void)
{
	return syscall(SYS_GET_EUID);
}

gid_t getegid(void)
{
	return syscall(SYS_GET_EGID);
}

int seteuid(uid_t uid)
{
	return syscall(SYS_SET_EUID, uid);
}

int setegid(gid_t gid)
{
	return syscall(SYS_SET_EGID, gid);
}

int setuid(uid_t uid)
{
	return syscall(SYS_SET_UID, uid);
}

int setgid(gid_t gid)
{
	return syscall(SYS_SET_GID, gid);
}

int setreuid(uid_t ruid, uid_t euid)
{
	return syscall(SYS_SET_REUID, ruid, euid);
}

int setregid(gid_t rgid, gid_t egid)
{
	return syscall(SYS_SET_REGID, rgid, egid);
}
