#include <BAN/Assert.h>
#include <kernel/Syscall.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

char** environ;

extern void init_malloc();
extern "C" void _init_libc(char** _environ)
{
	init_malloc();

	if (!_environ)
		return;

	size_t env_count = 0;
	while (_environ[env_count])
		env_count++;

	environ = (char**)malloc(sizeof(char*) * env_count + 1);
	for (size_t i = 0; i < env_count; i++)
	{
		size_t bytes = strlen(_environ[i]) + 1;
		environ[i] = (char*)malloc(bytes);
		memcpy(environ[i], _environ[i], bytes);
	}
	environ[env_count] = nullptr;
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

ssize_t readlink(const char* __restrict path, char* __restrict buf, size_t bufsize)
{
	return syscall(SYS_READLINK, path, buf, bufsize);
}

ssize_t readlinkat(int fd, const char* __restrict path, char* __restrict buf, size_t bufsize)
{
	return syscall(SYS_READLINKAT, fd, path, buf, bufsize);
}

ssize_t pread(int fildes, void* buf, size_t nbyte, off_t offset)
{
	return syscall(SYS_PREAD, fildes, buf, nbyte, offset);
}

off_t lseek(int fildes, off_t offset, int whence)
{
	return syscall(SYS_SEEK, fildes, offset, whence);
}

int ftruncate(int fildes, off_t length)
{
	return syscall(SYS_TRUNCATE, fildes, length);
}

int dup(int fildes)
{
	return syscall(SYS_DUP, fildes);
}

int dup2(int fildes, int fildes2)
{
	return syscall(SYS_DUP2, fildes, fildes2);
}

int isatty(int fildes)
{
	return syscall(SYS_ISATTY, fildes) >= 0;
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

int execvp(const char* file, char* const argv[])
{
	char buffer[1024];
	const char* pathname = NULL;

	// do path resolution if file doesn't contain /
	if (strchr(file, '/') == nullptr)
	{
		const char* cur = getenv("PATH");
		if (!cur)
		{
			errno = ENOENT;
			return -1;
		}

		while (*cur)
		{
			const char* end = strchrnul(cur, ':');
			size_t len = end - cur;

			ASSERT(strlen(file) + 1 + len < sizeof(buffer));

			strncpy(buffer, cur, len);
			strcat(buffer, "/");
			strcat(buffer, file);

			struct stat st;
			if (stat(buffer, &st) == 0)
			{
				pathname = buffer;
				break;
			}

			cur = end;
			if (*cur)
				cur++;
		}
	}
	else
	{
		pathname = file;
	}

	if (!pathname)
	{
		errno = ENOENT;
		return -1;
	}

	return execve(pathname, argv, environ);
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
	unsigned int ret = syscall(SYS_SLEEP, seconds);
	if (ret > 0)
		errno = EINTR;
	return ret;
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

int chown(const char* path, uid_t owner, gid_t group)
{
	return syscall(SYS_CHOWN, path, owner, group);
}

void sync(void)
{
	syscall(SYS_SYNC, false);
}

void syncsync(int should_block)
{
	syscall(SYS_SYNC, should_block);
}

int unlink(const char* path)
{
	return syscall(SYS_UNLINK, path);
}

int rmdir(const char* path)
{
	return syscall(SYS_UNLINK, path);
}

char* optarg = nullptr;
int opterr = 1;
int optind = 1;
int optopt = 0;

int getopt(int argc, char* const argv[], const char* optstring)
{
	if (optind >= argc)
		return -1;

	static int idx = 1;
	const char* current = argv[optind];

	// if "--" is encountered, no more options are parsed
	if (idx == -1)
		return -1;

	// if current is nullptr, does not start with '-' or is string "-", return -1
	if (current == nullptr || current[0] != '-' || current[1] == '\0')
		return -1;

	// if current points to string "--" increment optind and return -1
	if (current[1] == '-' && current[2] == '\0')
	{
		idx = -1;
		optind++;
		return -1;
	}

	for (size_t i = 0; optstring[i]; i++)
	{
		if (optstring[i] == ':')
			continue;
		if (current[idx] != optstring[i])
			continue;

		if (optstring[i + 1] == ':')
		{
			if (current[idx + 1])
			{
				optarg = const_cast<char*>(current + idx + 1);
				optind += 1;
			}
			else
			{
				optarg = const_cast<char*>(argv[optind + 1]);
				optind += 2;
			}

			idx = 1;

			if (optind > argc)
			{
				if (opterr && optstring[0] != ':')
					fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optstring[i]);
				optopt = optstring[i];
				return optstring[0] == ':' ? ':' : '?';
			}

			return optstring[i];
		}
		else
		{
			if (current[++idx] == '\0')
			{
				idx = 1;
				optind++;
			}

			return optstring[i];
		}
	}

	if (opterr && optstring[0] != ':')
		fprintf(stderr, "%s: illegal option -- %c\n", argv[0], current[idx]);

	if (current[++idx] == '\0')
	{
		idx = 1;
		optind++;
	}

	return '?';
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

char* getlogin(void)
{
	static char buffer[LOGIN_NAME_MAX];
	auto* pw = getpwuid(geteuid());
	if (pw == nullptr)
		return nullptr;
	strncpy(buffer, pw->pw_name, LOGIN_NAME_MAX - 1);
	buffer[LOGIN_NAME_MAX - 1] = '\0';
	endpwent();
	return buffer;
}

char* ttyname(int fildes)
{
	static char storage[_POSIX_TTY_NAME_MAX];
	if (syscall(SYS_TTYNAME, fildes, storage) == -1)
		return nullptr;
	return storage;
}

int access(const char* path, int amode)
{
	return syscall(SYS_ACCESS, path, amode);
}
