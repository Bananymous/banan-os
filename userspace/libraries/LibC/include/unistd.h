#ifndef _UNISTD_H
#define _UNISTD_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION -1
#define _XOPEN_VERSION 700

#define _POSIX_ADVISORY_INFO -1
#define _POSIX_ASYNCHRONOUS_IO -1
#define _POSIX_BARRIERS -1
#define _POSIX_CHOWN_RESTRICTED -1
#define _POSIX_CLOCK_SELECTION -1
#define _POSIX_CPUTIME -1
#define _POSIX_FSYNC -1
#define _POSIX_IPV6 -1
#define _POSIX_JOB_CONTROL -1
#define _POSIX_MAPPED_FILES -1
#define _POSIX_MEMLOCK -1
#define _POSIX_MEMLOCK_RANGE -1
#define _POSIX_MEMORY_PROTECTION -1
#define _POSIX_MESSAGE_PASSING -1
#define _POSIX_MONOTONIC_CLOCK -1
#define _POSIX_NO_TRUNC -1
#define _POSIX_PRIORITIZED_IO -1
#define _POSIX_PRIORITY_SCHEDULING -1
#define _POSIX_RAW_SOCKETS -1
#define _POSIX_READER_WRITER_LOCKS -1
#define _POSIX_REALTIME_SIGNALS -1
#define _POSIX_REGEXP -1
#define _POSIX_SAVED_IDS -1
#define _POSIX_SEMAPHORES -1
#define _POSIX_SHARED_MEMORY_OBJECTS -1
#define _POSIX_SHELL -1
#define _POSIX_SPAWN -1
#define _POSIX_SPIN_LOCKS -1
#define _POSIX_SPORADIC_SERVER -1
#define _POSIX_SYNCHRONIZED_IO xx
#define _POSIX_THREAD_ATTR_STACKADDR -1
#define _POSIX_THREAD_ATTR_STACKSIZE -1
#define _POSIX_THREAD_CPUTIME -1
#define _POSIX_THREAD_PRIO_INHERIT -1
#define _POSIX_THREAD_PRIO_PROTECT -1
#define _POSIX_THREAD_PRIORITY_SCHEDULING -1
#define _POSIX_THREAD_PROCESS_SHARED -1
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT -1
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT -1
#define _POSIX_THREAD_SAFE_FUNCTIONS -1
#define _POSIX_THREAD_SPORADIC_SERVER -1
#define _POSIX_THREADS -1
#define _POSIX_TIMEOUTS -1
#define _POSIX_TIMERS -1
#define _POSIX_TRACE -1
#define _POSIX_TRACE_EVENT_FILTER -1
#define _POSIX_TRACE_INHERIT -1
#define _POSIX_TRACE_LOG -1
#define _POSIX_TYPED_MEMORY_OBJECTS -1
#define _POSIX_V6_ILP32_OFF32 -1
#define _POSIX_V6_ILP32_OFFBIG -1
#define _POSIX_V6_LP64_OFF64 -1
#define _POSIX_V6_LPBIG_OFFBIG -1
#define _POSIX_V7_ILP32_OFF32 -1
#define _POSIX_V7_ILP32_OFFBIG -1
#define _POSIX_V7_LP64_OFF64 -1
#define _POSIX_V7_LPBIG_OFFBIG -1
#define _POSIX2_C_BIND -1
#define _POSIX2_C_DEV -1
#define _POSIX2_CHAR_TERM -1
#define _POSIX2_FORT_DEV -1
#define _POSIX2_FORT_RUN -1
#define _POSIX2_LOCALEDEF -1
#define _POSIX2_PBS -1
#define _POSIX2_PBS_ACCOUNTING -1
#define _POSIX2_PBS_CHECKPOINT -1
#define _POSIX2_PBS_LOCATE -1
#define _POSIX2_PBS_MESSAGE -1
#define _POSIX2_PBS_TRACK -1
#define _POSIX2_SW_DEV -1
#define _POSIX2_UPE -1
#define _XOPEN_CRYPT -1
#define _XOPEN_ENH_I18N -1
#define _XOPEN_REALTIME -1
#define _XOPEN_REALTIME_THREADS -1
#define _XOPEN_SHM -1
#define _XOPEN_STREAMS -1
#define _XOPEN_UNIX -1
#define _XOPEN_UUCP -1

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define __need_size_t
#define __need_ssize_t
#define __need_uid_t
#define __need_gid_t
#define __need_off_t
#define __need_pid_t
#define __need_useconds_t
#include <sys/types.h>

// FIXME: _CS prefixed definitions

enum
{
	_PC_2_SYMLINKS,
	_PC_ALLOC_SIZE_MIN,
	_PC_ASYNC_IO,
	_PC_CHOWN_RESTRICTED,
	_PC_FILESIZEBITS,
	_PC_LINK_MAX,
	_PC_MAX_CANON,
	_PC_MAX_INPUT,
	_PC_NAME_MAX,
	_PC_NO_TRUNC,
	_PC_PATH_MAX,
	_PC_PIPE_BUF,
	_PC_PRIO_IO,
	_PC_REC_INCR_XFER_SIZE,
	_PC_REC_MAX_XFER_SIZE,
	_PC_REC_MIN_XFER_SIZE,
	_PC_REC_XFER_ALIGN,
	_PC_SYMLINK_MAX,
	_PC_SYNC_IO,
	_PC_TIMESTAMP_RESOLUTION,
	_PC_VDISABLE,
};

#define F_OK 0x01
#define R_OK 0x02
#define W_OK 0x04
#define X_OK 0x08

#define F_LOCK	0
#define F_TEST	1
#define F_TLOCK	2
#define F_ULOCK	3

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define STDDBG_FILENO 3

#define _POSIX_VDISABLE 0

int					access(const char* path, int amode);
unsigned			alarm(unsigned seconds);
int					chdir(const char* path);
int					chown(const char* path, uid_t owner, gid_t group);
int					close(int fildes);
size_t				confstr(int name, char* buf, size_t len);
char*				crypt(const char* key, const char* salt);
int					dup(int fildes);
int					dup2(int fildes, int fildes2);
void				_exit(int status) __attribute__((__noreturn__));
void				encrypt(char block[64], int edflag);
int					execl(const char* path, const char* arg0, ...);
int					execle(const char* path, const char* arg0, ...);
int					execlp(const char* file, const char* arg0, ...);
int					execv(const char* path, char* const argv[]);
int					execve(const char* path, char* const argv[], char* const envp[]);
int					execvp(const char* file, char* const argv[]);
int					faccessat(int fd, const char* path, int amode, int flag);
int					fchdir(int fildes);
int					fchown(int fildes, uid_t owner, gid_t group);
int					fchownat(int fd, const char* path, uid_t owner, gid_t group, int flag);
int					fdatasync(int fildes);
int					fexecve(int fd, char* const argv[], char* const envp[]);
pid_t				fork(void);
long				fpathconf(int fildes, int name);
int					fsync(int fildes);
int					ftruncate(int fildes, off_t length);
char*				getcwd(char* buf , size_t size);
gid_t				getegid(void);
uid_t				geteuid(void);
gid_t				getgid(void);
int					getgroups(int gidsetsize, gid_t grouplist[]);
long				gethostid(void);
int					gethostname(char* name, size_t namelen);
char*				getlogin(void);
int					getlogin_r(char* name, size_t namesize);
int					getopt(int argc, char* const argv[], const char* optstring);
pid_t				getpgid(pid_t pid);
pid_t				getpgrp(void);
pid_t				getpid(void);
pid_t				getppid(void);
pid_t				getsid(pid_t pid);
uid_t				getuid(void);
int					isatty(int fildes);
int					lchown(const char* path, uid_t owner, gid_t group);
int					link(const char* path1, const char* path2);
int					linkat(int fd1, const char* path1, int fd2, const char* path2, int flag);
int					lockf(int fildes, int function, off_t size);
off_t				lseek(int fildes, off_t offset, int whence);
int					nice(int incr);
long				pathconf(const char* path, int name);
int					pause(void);
int					pipe(int fildes[2]);
ssize_t				pread(int fildes, void* buf, size_t nbyte, off_t offset);
ssize_t				pwrite(int fildes, const void* buf, size_t nbyte, off_t offset);
ssize_t				read(int fildes, void* buf, size_t nbyte);
ssize_t				readlink(const char* __restrict path, char* __restrict buf, size_t bufsize);
ssize_t				readlinkat(int fd, const char* __restrict path, char* __restrict buf, size_t bufsize);
int					rmdir(const char* path);
int					setegid(gid_t gid);
int					seteuid(uid_t uid);
int					setgid(gid_t gid);
int					setpgid(pid_t pid, pid_t pgid);
pid_t				setpgrp(void);
int					setregid(gid_t rgid, gid_t egid);
int					setreuid(uid_t ruid, uid_t euid);
pid_t				setsid(void);
int					setuid(uid_t uid);
unsigned			sleep(unsigned seconds);
void				swab(const void* __restrict src, void* __restrict dest, ssize_t nbytes);
int					symlink(const char* path1, const char* path2);
int					symlinkat(const char* path1, int fd, const char* path2);
void				sync(void);
void				syncsync(int should_block);
long				sysconf(int name);
pid_t				tcgetpgrp(int fildes);
int					tcsetpgrp(int fildes, pid_t pgid_id);
int					truncate(const char* path, off_t length);
char*				ttyname(int fildes);
int					ttyname_r(int fildes, char* name, size_t namesize);
int					unlink(const char* path);
int					unlinkat(int fd, const char* path, int flag);
int					usleep(useconds_t usec);
ssize_t				write(int fildes, const void* buf, size_t nbyte);

int					getpagesize(void);

extern char*	optarg;
extern int		opterr, optind, optopt;

long syscall(long syscall, ...);

__END_DECLS

#endif
