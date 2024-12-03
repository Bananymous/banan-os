#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <kernel/Memory/Types.h>
#include <kernel/Syscall.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

char** environ;

extern void _init_malloc();
extern void _init_stdio();
extern "C" void _init_libc(char** _environ)
{
	static bool is_initialized = false;
	if (is_initialized)
		return;
	is_initialized = true;

	_init_malloc();
	_init_stdio();

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
	return readlinkat(AT_FDCWD, path, buf, bufsize);
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

int fsync(int fildes)
{
	return syscall(SYS_FSYNC, fildes);
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

int gethostname(char* name, size_t namelen)
{
	FILE* fp = fopen("/etc/hostname", "r");
	if (fp == NULL)
		return -1;
	size_t nread = fread(name, namelen - 1, 1, fp);
	while (nread > 0 && name[nread - 1] == '\n')
		nread--;
	name[nread] = '\0';
	fclose(fp);
	return 0;
}

static int exec_impl(const char* pathname, char* const* argv, char* const* envp, bool do_path_resolution)
{
	char buffer[PATH_MAX];

	if (do_path_resolution && strchr(pathname, '/') == nullptr)
	{
		const char* cur = getenv("PATH");
		if (!cur)
		{
			errno = ENOENT;
			return -1;
		}

		char* resolved = nullptr;
		while (*cur)
		{
			const char* end = strchrnul(cur, ':');
			size_t len = end - cur;

			ASSERT(strlen(pathname) + 1 + len < sizeof(buffer));

			strncpy(buffer, cur, len);
			strcat(buffer, "/");
			strcat(buffer, pathname);

			struct stat st;
			if (stat(buffer, &st) == 0)
			{
				resolved = buffer;
				break;
			}

			cur = end;
			if (*cur)
				cur++;
		}

		if (!resolved)
		{
			errno = ENOENT;
			return -1;
		}

		pathname = resolved;
	}

	return syscall(SYS_EXEC, pathname, argv, envp);
}

static int execl_impl(const char* pathname, const char* arg0, va_list ap, bool has_env, bool do_path_resolution)
{
	int argc = 1;

	va_list ap2;
	va_copy(ap2, ap);
	while (va_arg(ap2, char*))
		argc++;
	va_end(ap2);

	char** argv = static_cast<char**>(malloc((argc + 1) * sizeof(char*)));
	if (argv == nullptr)
	{
		errno = ENOMEM;
		return -1;
	}

	argv[0] = const_cast<char*>(arg0);
	for (int i = 1; i < argc; i++)
		argv[i] = va_arg(ap, char*);
	argv[argc] = nullptr;

	char** envp = environ;
	if (has_env)
	{
		va_arg(ap, char*);
		envp = va_arg(ap, char**);;
	}

	return exec_impl(pathname, argv, envp, do_path_resolution);
}

int execl(const char* pathname, const char* arg0, ...)
{
	va_list ap;
	va_start(ap, arg0);
	int ret = execl_impl(pathname, arg0, ap, false, false);
	va_end(ap);
	return ret;
}

int execlp(const char* pathname, const char* arg0, ...)
{
	va_list ap;
	va_start(ap, arg0);
	int ret = execl_impl(pathname, arg0, ap, false, true);
	va_end(ap);
	return ret;
}

int execle(const char* pathname, const char* arg0, ...)
{
	va_list ap;
	va_start(ap, arg0);
	int ret = execl_impl(pathname, arg0, ap, true, false);
	va_end(ap);
	return ret;
}

int execv(const char* pathname, char* const argv[])
{
	return exec_impl(pathname, argv, environ, false);
}

int execve(const char* pathname, char* const argv[], char* const envp[])
{
	return exec_impl(pathname, argv, envp, false);
}

int execvp(const char* pathname, char* const argv[])
{
	return exec_impl(pathname, argv, environ, true);
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

int usleep(useconds_t usec)
{
	timespec ts;
	ts.tv_sec = usec / 1'000'000;
	ts.tv_nsec = (usec % 1'000'000) * 1000;
	return nanosleep(&ts, nullptr);
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
	return fchownat(AT_FDCWD, path, owner, group, 0);
}

int lchown(const char* path, uid_t owner, gid_t group)
{
	return fchownat(AT_FDCWD, path, owner, group, AT_SYMLINK_NOFOLLOW);
}

int fchown(int fildes, uid_t owner, gid_t group)
{
	return fchownat(fildes, nullptr, owner, group, 0);
}

int fchownat(int fd, const char* path, uid_t owner, gid_t group, int flag)
{
	return syscall(SYS_FCHOWNAT, fd, path, owner, group, flag);
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

int getpagesize(void)
{
	return PAGE_SIZE;
}

char* getpass(const char* prompt)
{
	static char buffer[PASS_MAX];

	int fd = open("/dev/tty", O_RDWR);
	if (fd == -1)
		return NULL;

	termios orig, temp;
	tcgetattr(fd, &orig);

	char* ret = nullptr;
	ssize_t total_read = 0;

	if (write(fd, prompt, strlen(prompt)) < 0)
		goto error;

	temp = orig;
	temp.c_lflag &= ~ECHO;
	tcsetattr(fd, TCSANOW, &temp);

	while (total_read == 0 || buffer[total_read - 1] != '\n')
	{
		ssize_t nread = read(fd, buffer + total_read, sizeof(buffer) - total_read - 1);
		if (nread < 0)
			goto error;
		total_read += nread;
	}

	buffer[total_read - 1] = '\0';
	ret = buffer;

	write(fd, "\n", 1);

error:
	tcsetattr(fd, TCSANOW, &orig);
	close(fd);
	return ret;
}

pid_t getppid(void)
{
	return syscall(SYS_GET_PPID);
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

int tcgetpgrp(int fildes)
{
	return syscall(SYS_TCGETPGRP, fildes);
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

int setsid(void)
{
	return syscall(SYS_SET_SID);
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

unsigned alarm(unsigned seconds)
{
	itimerval value, ovalue;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &value, &ovalue);
	return ovalue.it_value.tv_sec;
}

int symlink(const char* path1, const char* path2)
{
	return symlinkat(path1, AT_FDCWD, path2);
}

int symlinkat(const char* path1, int fd, const char* path2)
{
	return syscall(SYS_SYMLINKAT, path1, fd, path2);
}

int link(const char* path1, const char* path2)
{
	return linkat(AT_FDCWD, path1, AT_FDCWD, path2, 0);
}

int linkat(int fd1, const char *path1, int fd2, const char *path2, int flag)
{
	return syscall(SYS_HARDLINKAT, fd1, path1, fd2, path2, flag);
}
