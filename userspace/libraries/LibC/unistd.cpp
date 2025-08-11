#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <BAN/StringView.h>

#include <kernel/Memory/Types.h>
#include <kernel/Syscall.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

struct init_funcs_t
{
	void (*func)();
	void (**array_start)();
	void (**array_end)();
};

extern "C" char** environ;

extern "C" void _init_libc(char** environ, init_funcs_t init_funcs, init_funcs_t fini_funcs)
{
	if (::environ == nullptr)
		::environ = environ;

	if (uthread* self = reinterpret_cast<uthread*>(syscall(SYS_GET_TLS)))
	{
		self->cleanup_stack = nullptr;
		self->id = syscall(SYS_PTHREAD_SELF);
		self->errno_ = 0;
		self->cancel_type = PTHREAD_CANCEL_DEFERRED;
		self->cancel_state = PTHREAD_CANCEL_ENABLE;
		self->canceled = false;
	}
	else
	{
		alignas(uthread) static uint8_t storage[sizeof(uthread) + sizeof(uintptr_t)];

		uthread& uthread = *reinterpret_cast<struct uthread*>(storage);
		uthread = {
			.self = &uthread,
			.master_tls_addr = nullptr,
			.master_tls_size = 0,
			.cleanup_stack = nullptr,
			.id = static_cast<pthread_t>(syscall(SYS_PTHREAD_SELF)),
			.errno_ = 0,
			.cancel_type = PTHREAD_CANCEL_DEFERRED,
			.cancel_state = PTHREAD_CANCEL_ENABLE,
			.canceled = false,
		};
		uthread.dtv[0] = 0;

		syscall(SYS_SET_TLS, &uthread);
	}

	// call global constructors
	if (init_funcs.func)
		init_funcs.func();
	const size_t init_array_count = init_funcs.array_end - init_funcs.array_start;
	for (size_t i = 0; i < init_array_count; i++)
		init_funcs.array_start[i]();

	// register global destructors
	const size_t fini_array_count = fini_funcs.array_end - fini_funcs.array_start;
	for (size_t i = 0; i < fini_array_count; i++)
		atexit(fini_funcs.array_start[i]);
	if (fini_funcs.func)
		atexit(fini_funcs.func);
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

	long ret;
	do
		ret = Kernel::syscall(syscall, arg1, arg2, arg3, arg4, arg5);
	while (ret == -ERESTART);

	if (ret < 0)
	{
		errno = -ret;
		return -1;
	}

	return ret;
}

int close(int fd)
{
	pthread_testcancel();
	return syscall(SYS_CLOSE, fd);
}

ssize_t read(int fildes, void* buf, size_t nbyte)
{
	pthread_testcancel();
	return syscall(SYS_READ, fildes, buf, nbyte);
}

ssize_t write(int fildes, const void* buf, size_t nbyte)
{
	pthread_testcancel();
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
	pthread_testcancel();
	return syscall(SYS_PREAD, fildes, buf, nbyte, offset);
}

ssize_t pwrite(int fildes, const void* buf, size_t nbyte, off_t offset)
{
	pthread_testcancel();
	return syscall(SYS_PWRITE, fildes, buf, nbyte, offset);
}

off_t lseek(int fildes, off_t offset, int whence)
{
	return syscall(SYS_SEEK, fildes, offset, whence);
}

int truncate(const char* path, off_t length)
{
	const int fd = open(path, O_WRONLY);
	if (fd == -1)
		return -1;
	int ret = ftruncate(fd, length);
	close(fd);
	return ret;
}

int ftruncate(int fildes, off_t length)
{
	return syscall(SYS_FTRUNCATE, fildes, length);
}

int fsync(int fildes)
{
	pthread_testcancel();
	return syscall(SYS_FSYNC, fildes);
}

int dup(int fildes)
{
	return fcntl(fildes, F_DUPFD, 0);
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
	size_t nread = fread(name, 1, namelen - 1, fp);
	while (nread > 0 && name[nread - 1] == '\n')
		nread--;
	name[nread] = '\0';
	fclose(fp);
	return 0;
}

static int exec_impl_shebang(FILE* fp, const char* pathname, char* const* argv, char* const* envp, int shebang_depth);

static int exec_impl(const char* pathname, char* const* argv, char* const* envp, bool do_path_resolution, int shebang_depth = 0)
{
	if (shebang_depth > 100)
	{
		errno = ELOOP;
		return -1;
	}

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
			const size_t len = end - cur;

			ASSERT(strlen(pathname) + 1 + len < sizeof(buffer));

			memcpy(buffer, cur, len);
			buffer[len] = '/';
			strcpy(buffer + len + 1, pathname);

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

	if (access(pathname, X_OK) == -1)
		return -1;

	if (FILE* fp = fopen(pathname, "r"))
	{
		char shebang[2];
		if (fread(shebang, 1, 2, fp) == 2 && shebang[0] == '#' && shebang[1] == '!')
			return exec_impl_shebang(fp, pathname, argv, envp, shebang_depth);
		fclose(fp);
	}

	return syscall(SYS_EXEC, pathname, argv, envp);
}

static int exec_impl_shebang(FILE* fp, const char* pathname, char* const* argv, char* const* envp, int shebang_depth)
{
	constexpr size_t buffer_len = PATH_MAX + 1 + ARG_MAX + 1;
	char* buffer = static_cast<char*>(malloc(buffer_len));
	if (buffer == nullptr)
	{
		fclose(fp);
		return -1;
	}

	if (fgets(buffer, buffer_len, fp) == nullptr)
	{
		free(buffer);
		return -1;
	}

	const auto sv_trim_whitespace =
		[](BAN::StringView sv) -> BAN::StringView
		{
			while (!sv.empty() && isspace(sv.front()))
				sv = sv.substring(1);
			while (!sv.empty() && isspace(sv.back()))
				sv = sv.substring(0, sv.size() - 1);
			return sv;
		};

	BAN::StringView buffer_sv = buffer;
	if (buffer_sv.back() != '\n')
	{
		free(buffer);
		errno = ENOEXEC;
		return -1;
	}
	buffer_sv = sv_trim_whitespace(buffer_sv);

	BAN::StringView interpreter, argument;
	if (auto space = buffer_sv.find([](char ch) -> bool { return isspace(ch); }); !space.has_value())
		interpreter = buffer_sv;
	else
	{
		interpreter = sv_trim_whitespace(buffer_sv.substring(0, space.value()));
		argument = sv_trim_whitespace(buffer_sv.substring(space.value()));
	}

	if (interpreter.empty())
	{
		free(buffer);
		errno = ENOEXEC;
		return -1;
	}

	// null terminate interpreter and argument
	const_cast<char*>(interpreter.data())[interpreter.size()] = '\0';
	if (!argument.empty())
		const_cast<char*>(argument.data())[argument.size()] = '\0';

	size_t old_argc = 0;
	while (argv[old_argc])
		old_argc++;

	const size_t extra_args = 1 + !argument.empty();
	char** new_argv = static_cast<char**>(malloc((extra_args + old_argc + 1) * sizeof(char*)));;
	if (new_argv == nullptr)
	{
		free(buffer);
		return -1;
	}

	new_argv[0] = const_cast<char*>(pathname);
	if (!argument.empty())
		new_argv[1] = const_cast<char*>(argument.data());
	for (size_t i = 0; i < old_argc; i++)
		new_argv[i + extra_args] = argv[i];
	new_argv[old_argc + extra_args] = nullptr;

	exec_impl(interpreter.data(), new_argv, envp, true, shebang_depth + 1);
	free(new_argv);
	free(buffer);
	return -1;
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

	exec_impl(pathname, argv, envp, do_path_resolution);
	free(argv);
	return -1;
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
	_pthread_call_atfork(_PTHREAD_ATFORK_PREPARE);
	const pid_t pid = syscall(SYS_FORK);
	if (pid == -1)
		return -1;
	_pthread_call_atfork(pid ? _PTHREAD_ATFORK_PARENT : _PTHREAD_ATFORK_CHILD);
	return pid;
}

int pipe(int fildes[2])
{
	return syscall(SYS_PIPE, fildes);
}

unsigned int sleep(unsigned int seconds)
{
	pthread_testcancel();
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

	if (syscall(SYS_GETCWD, buf, size) == 0)
		return nullptr;

	return buf;
}

int chdir(const char* path)
{
	return syscall(SYS_CHDIR, path);
}

int fchdir(int fildes)
{
	return syscall(SYS_FCHDIR, fildes);
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

int fdatasync(int fildes)
{
	pthread_testcancel();

	(void)fildes;
	dprintln("TODO: fdatasync");
	return syscall(SYS_SYNC, true);
}

int unlink(const char* path)
{
	return unlinkat(AT_FDCWD, path, 0);
}

int unlinkat(int fd, const char* path, int flag)
{
	return syscall(SYS_UNLINKAT, fd, path, flag);
}

int rmdir(const char* path)
{
	return unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
}

int chroot(const char* path)
{
	return syscall(SYS_CHROOT, path);
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

int getgroups(int gidsetsize, gid_t grouplist[])
{
	return syscall(SYS_GETGROUPS, grouplist, gidsetsize);
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
	static char storage[TTY_NAME_MAX];
	if (ttyname_r(fildes, storage, sizeof(storage)) != 0)
		return nullptr;
	return storage;
}

int ttyname_r(int fildes, char* name, size_t namesize)
{
	if (syscall(SYS_TTYNAME, fildes, name, namesize))
		return errno;
	return 0;
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

size_t confstr(int name, char* buf, size_t len)
{
	(void)buf;
	(void)len;

	switch (name)
	{
#define CONFSTR_CASE(name) case name: return 0;
		CONFSTR_CASE(_CS_PATH)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFF32_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFF32_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFF32_LIBS)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFFBIG_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_ILP32_OFFBIG_LIBS)
		CONFSTR_CASE(_CS_POSIX_V6_LP64_OFF64_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_LP64_OFF64_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_LP64_OFF64_LIBS)
		CONFSTR_CASE(_CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V6_LPBIG_OFFBIG_LIBS)
		CONFSTR_CASE(_CS_POSIX_V6_WIDTH_RESTRICTED_ENVS)
		CONFSTR_CASE(_CS_V6_ENV)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFF32_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFF32_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFF32_LIBS)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFFBIG_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_ILP32_OFFBIG_LIBS)
		CONFSTR_CASE(_CS_POSIX_V7_LP64_OFF64_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_LP64_OFF64_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_LP64_OFF64_LIBS)
		CONFSTR_CASE(_CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_LPBIG_OFFBIG_LIBS)
		CONFSTR_CASE(_CS_POSIX_V7_THREADS_CFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_THREADS_LDFLAGS)
		CONFSTR_CASE(_CS_POSIX_V7_WIDTH_RESTRICTED_ENVS)
		CONFSTR_CASE(_CS_V7_ENV)
#undef CONFSTR_CASE
	}

	errno = EINVAL;
	return 0;
}

long fpathconf(int fd, int name)
{
	(void)fd;
	switch (name)
	{
#define LIMIT_CASE(name) case _PC_##name: return name;
		LIMIT_CASE(LINK_MAX)
		LIMIT_CASE(MAX_CANON)
		LIMIT_CASE(MAX_INPUT)
		LIMIT_CASE(NAME_MAX)
		LIMIT_CASE(PATH_MAX)
		LIMIT_CASE(PIPE_BUF)
#undef LIMIT_CASE
#define POSIX_CASE(name) case _PC_##name: return _POSIX_##name;
		POSIX_CASE(CHOWN_RESTRICTED)
		POSIX_CASE(NO_TRUNC)
		POSIX_CASE(VDISABLE)
#undef POSIX_CASE
	}

	errno = EINVAL;
	return 0;
}

long pathconf(const char* path, int name)
{
	(void)path;
	return fpathconf(0, name);
}

long sysconf(int name)
{
	switch (name)
	{
#define POSIX2_CASE(name) case _SC_2_##name: return _POSIX2_##name;
		POSIX2_CASE(C_BIND)
		POSIX2_CASE(C_DEV)
		POSIX2_CASE(CHAR_TERM)
		POSIX2_CASE(FORT_DEV)
		POSIX2_CASE(FORT_RUN)
		POSIX2_CASE(LOCALEDEF)
		POSIX2_CASE(PBS)
		POSIX2_CASE(PBS_ACCOUNTING)
		POSIX2_CASE(PBS_CHECKPOINT)
		POSIX2_CASE(PBS_LOCATE)
		POSIX2_CASE(PBS_MESSAGE)
		POSIX2_CASE(PBS_TRACK)
		POSIX2_CASE(SW_DEV)
		POSIX2_CASE(UPE)
		POSIX2_CASE(VERSION)
#undef POSIX2_CASE

#define POSIX_CASE(name) case _SC_##name: return _POSIX_##name;
		POSIX_CASE(ADVISORY_INFO)
		POSIX_CASE(AIO_LISTIO_MAX)
		POSIX_CASE(AIO_MAX)
		POSIX_CASE(ARG_MAX)
		POSIX_CASE(ASYNCHRONOUS_IO)
		POSIX_CASE(BARRIERS)
		POSIX_CASE(CHILD_MAX)
		POSIX_CASE(CLOCK_SELECTION)
		POSIX_CASE(CPUTIME)
		POSIX_CASE(DELAYTIMER_MAX)
		POSIX_CASE(FSYNC)
		POSIX_CASE(HOST_NAME_MAX)
		POSIX_CASE(IPV6)
		POSIX_CASE(JOB_CONTROL)
		POSIX_CASE(LOGIN_NAME_MAX)
		POSIX_CASE(MAPPED_FILES)
		POSIX_CASE(MEMLOCK)
		POSIX_CASE(MEMLOCK_RANGE)
		POSIX_CASE(MEMORY_PROTECTION)
		POSIX_CASE(MESSAGE_PASSING)
		POSIX_CASE(MONOTONIC_CLOCK)
		POSIX_CASE(MQ_OPEN_MAX)
		POSIX_CASE(MQ_PRIO_MAX)
		POSIX_CASE(NGROUPS_MAX)
		POSIX_CASE(OPEN_MAX)
		POSIX_CASE(PRIORITIZED_IO)
		POSIX_CASE(PRIORITY_SCHEDULING)
		POSIX_CASE(RAW_SOCKETS)
		POSIX_CASE(RE_DUP_MAX)
		POSIX_CASE(READER_WRITER_LOCKS)
		POSIX_CASE(REALTIME_SIGNALS)
		POSIX_CASE(REGEXP)
		POSIX_CASE(RTSIG_MAX)
		POSIX_CASE(SAVED_IDS)
		POSIX_CASE(SEM_NSEMS_MAX)
		POSIX_CASE(SEM_VALUE_MAX)
		POSIX_CASE(SEMAPHORES)
		POSIX_CASE(SHARED_MEMORY_OBJECTS)
		POSIX_CASE(SHELL)
		POSIX_CASE(SIGQUEUE_MAX)
		POSIX_CASE(SPAWN)
		POSIX_CASE(SPIN_LOCKS)
		POSIX_CASE(SPORADIC_SERVER)
		POSIX_CASE(SS_REPL_MAX)
		POSIX_CASE(STREAM_MAX)
		POSIX_CASE(SYMLOOP_MAX)
		POSIX_CASE(SYNCHRONIZED_IO)
		POSIX_CASE(THREAD_ATTR_STACKADDR)
		POSIX_CASE(THREAD_ATTR_STACKSIZE)
		POSIX_CASE(THREAD_CPUTIME)
		POSIX_CASE(THREAD_DESTRUCTOR_ITERATIONS)
		POSIX_CASE(THREAD_KEYS_MAX)
		POSIX_CASE(THREAD_PRIO_INHERIT)
		POSIX_CASE(THREAD_PRIO_PROTECT)
		POSIX_CASE(THREAD_PRIORITY_SCHEDULING)
		POSIX_CASE(THREAD_PROCESS_SHARED)
		POSIX_CASE(THREAD_ROBUST_PRIO_INHERIT)
		POSIX_CASE(THREAD_ROBUST_PRIO_PROTECT)
		POSIX_CASE(THREAD_SAFE_FUNCTIONS)
		POSIX_CASE(THREAD_SPORADIC_SERVER)
		POSIX_CASE(THREAD_THREADS_MAX)
		POSIX_CASE(THREADS)
		POSIX_CASE(TIMEOUTS)
		POSIX_CASE(TIMER_MAX)
		POSIX_CASE(TIMERS)
		POSIX_CASE(TRACE)
		POSIX_CASE(TRACE_EVENT_FILTER)
		POSIX_CASE(TRACE_EVENT_NAME_MAX)
		POSIX_CASE(TRACE_INHERIT)
		POSIX_CASE(TRACE_LOG)
		POSIX_CASE(TRACE_NAME_MAX)
		POSIX_CASE(TRACE_SYS_MAX)
		POSIX_CASE(TRACE_USER_EVENT_MAX)
		POSIX_CASE(TTY_NAME_MAX)
		POSIX_CASE(TYPED_MEMORY_OBJECTS)
		POSIX_CASE(TZNAME_MAX)
		POSIX_CASE(V7_ILP32_OFF32)
		POSIX_CASE(V7_ILP32_OFFBIG)
		POSIX_CASE(V7_LP64_OFF64)
		POSIX_CASE(V7_LPBIG_OFFBIG)
		POSIX_CASE(V6_ILP32_OFF32)
		POSIX_CASE(V6_ILP32_OFFBIG)
		POSIX_CASE(V6_LP64_OFF64)
		POSIX_CASE(V6_LPBIG_OFFBIG)
		POSIX_CASE(VERSION)
#undef POSIX_CASE

#define LIMITS_CASE(name) case _SC_##name: return name;
		LIMITS_CASE(AIO_PRIO_DELTA_MAX)
		LIMITS_CASE(ATEXIT_MAX)
		LIMITS_CASE(BC_BASE_MAX)
		LIMITS_CASE(BC_DIM_MAX)
		LIMITS_CASE(BC_SCALE_MAX)
		LIMITS_CASE(BC_STRING_MAX)
		LIMITS_CASE(COLL_WEIGHTS_MAX)
		LIMITS_CASE(EXPR_NEST_MAX)
		LIMITS_CASE(IOV_MAX)
		LIMITS_CASE(LINE_MAX)
#undef LIMITS_CASE

#define XOPEN_CASE(name) case _SC_XOPEN_##name: return _XOPEN_##name;
		XOPEN_CASE(CRYPT)
		XOPEN_CASE(ENH_I18N)
		XOPEN_CASE(REALTIME)
		XOPEN_CASE(REALTIME_THREADS)
		XOPEN_CASE(SHM)
		XOPEN_CASE(STREAMS)
		XOPEN_CASE(UNIX)
		XOPEN_CASE(UUCP)
		XOPEN_CASE(VERSION)
#undef XOPEN_CASE

		case _SC_PAGE_SIZE:
		case _SC_PAGESIZE:         return getpagesize();

		case _SC_NPROCESSORS_ONLN: return syscall(SYS_GET_NPROCESSOR);
		case _SC_NPROCESSORS_CONF: return syscall(SYS_GET_NPROCESSOR);

		case _SC_CLK_TCK:          return 100;
		case _SC_GETGR_R_SIZE_MAX: return 1024;
		case _SC_GETPW_R_SIZE_MAX: return 1024;
		case _SC_THREAD_STACK_MIN: return PTHREAD_STACK_MIN;
	}

	errno = EINVAL;
	return -1;
}
