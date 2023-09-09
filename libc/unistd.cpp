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

	uintptr_t arg1 = va_arg(args, uintptr_t);
	uintptr_t arg2 = va_arg(args, uintptr_t);
	uintptr_t arg3 = va_arg(args, uintptr_t);
	uintptr_t arg4 = va_arg(args, uintptr_t);
	uintptr_t arg5 = va_arg(args, uintptr_t);

	va_end(args);

	long ret = Kernel::syscall(syscall, arg1, arg2, arg3, arg4, arg5);

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

int dup(int fildes)
{
	return syscall(SYS_DUP, fildes);
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

pid_t getpid(void)
{
	return syscall(SYS_GET_PID);
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

pid_t getpgrp(void)
{
	return getpgid(0);
}

pid_t getpgid(pid_t pid)
{
	return syscall(SYS_GET_PGID, pid);
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

pid_t setpgrp(void)
{
	setpgid(0, 0);
	return getpgrp();
}

int setpgid(pid_t pid, pid_t pgid)
{
	return syscall(SYS_SET_PGID, pid, pgid);
}

int tcsetpgrp(int fildes, pid_t pgid_id)
{
	return syscall(SYS_TCSETPGRP, fildes, pgid_id);
}
