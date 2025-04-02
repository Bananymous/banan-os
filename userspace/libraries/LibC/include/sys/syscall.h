#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __SYSCALL_LIST(O)					\
	O(SYS_EXIT,				exit)			\
	O(SYS_READ,				read)			\
	O(SYS_WRITE,			write)			\
	O(SYS_TERMID,			termid)			\
	O(SYS_CLOSE,			close)			\
	O(SYS_OPENAT,			openat)			\
	O(SYS_SEEK,				seek)			\
	O(SYS_TELL,				tell)			\
	O(SYS_TCGETATTR,		tcgetattr)		\
	O(SYS_TCSETATTR,		tcsetattr)		\
	O(SYS_FORK,				fork)			\
	O(SYS_EXEC,				exec)			\
	O(SYS_SLEEP,			sleep)			\
	O(SYS_WAIT,				wait)			\
	O(SYS_READ_DIR,			readdir)		\
	O(SYS_SET_UID,			setuid)			\
	O(SYS_SET_GID,			setgid)			\
	O(SYS_SET_SID,			setsid)			\
	O(SYS_SET_EUID,			seteuid)		\
	O(SYS_SET_EGID,			setegid)		\
	O(SYS_SET_REUID,		setreuid)		\
	O(SYS_SET_REGID,		setregid)		\
	O(SYS_GET_UID,			getuid)			\
	O(SYS_GET_GID,			getgid)			\
	O(SYS_GET_EUID,			geteuid)		\
	O(SYS_GET_EGID,			getegid)		\
	O(SYS_GET_PWD,			getpwd)			\
	O(SYS_SET_PWD,			setpwd)			\
	O(SYS_CLOCK_GETTIME,	clock_gettime)	\
	O(SYS_PIPE,				pipe)			\
	O(SYS_DUP2,				dup2)			\
	O(SYS_KILL,				kill)			\
	O(SYS_TCGETPGRP,		tcgetpgrp)		\
	O(SYS_TCSETPGRP,		tcsetpgrp)		\
	O(SYS_GET_PPID,			getppid)		\
	O(SYS_GET_PID,			getpid)			\
	O(SYS_GET_PGID,			getpgid)		\
	O(SYS_SET_PGID,			setpgid)		\
	O(SYS_FCNTL,			fcntl)			\
	O(SYS_NANOSLEEP,		nanosleep)		\
	O(SYS_FSTATAT,			fstatat)		\
	O(SYS_FSTATVFSAT,		fstatvfsat)		\
	O(SYS_SYNC,				sync)			\
	O(SYS_MMAP,				mmap)			\
	O(SYS_MUNMAP,			munmap) 		\
	O(SYS_TTY_CTRL,			tty_ctrl)		\
	O(SYS_POWEROFF,			poweroff)		\
	O(SYS_FCHMODAT,			fchmodat)		\
	O(SYS_CREATE_DIR,		create_dir)		\
	O(SYS_UNLINK,			unlink) 		\
	O(SYS_READLINKAT,		readlinkat)		\
	O(SYS_MSYNC,			msync)			\
	O(SYS_PREAD,			pread)			\
	O(SYS_FCHOWNAT,			fchownat)		\
	O(SYS_LOAD_KEYMAP,		load_keymap)	\
	O(SYS_SOCKET,			socket)			\
	O(SYS_BIND,				bind)			\
	O(SYS_SENDTO,			sendto)			\
	O(SYS_RECVFROM,			recvfrom)		\
	O(SYS_IOCTL,			ioctl)			\
	O(SYS_ACCEPT,			accept)			\
	O(SYS_CONNECT,			connect)		\
	O(SYS_LISTEN,			listen)			\
	O(SYS_PSELECT,			pselect)		\
	O(SYS_TRUNCATE,			truncate)		\
	O(SYS_SMO_CREATE,		smo_create)		\
	O(SYS_SMO_DELETE,		smo_delete)		\
	O(SYS_SMO_MAP,			smo_map)		\
	O(SYS_ISATTY,			isatty)			\
	O(SYS_GETSOCKNAME,		getsockname)	\
	O(SYS_GETSOCKOPT,		getsockopt)		\
	O(SYS_SETSOCKOPT,		setsockopt)		\
	O(SYS_REALPATH,			realpath)		\
	O(SYS_TTYNAME,			ttyname)		\
	O(SYS_ACCESS,			access)			\
	O(SYS_SIGACTION,		sigaction)		\
	O(SYS_SIGPENDING,		sigpending)		\
	O(SYS_SIGPROCMASK,		sigprocmask)	\
	O(SYS_SETITIMER,		setitimer)		\
	O(SYS_POSIX_OPENPT,		posix_openpt)	\
	O(SYS_PTSNAME,			ptsname)		\
	O(SYS_FSYNC,			fsync)			\
	O(SYS_SYMLINKAT,		symlinkat)		\
	O(SYS_HARDLINKAT,		hardlinkat)		\
	O(SYS_YIELD,			yield)			\
	O(SYS_PTHREAD_CREATE,	pthread_create)	\
	O(SYS_PTHREAD_EXIT,		pthread_exit)	\
	O(SYS_PTHREAD_JOIN,		pthread_join)	\
	O(SYS_PTHREAD_SELF,		pthread_self)	\

enum Syscall
{
#define O(enum, name) enum,
	__SYSCALL_LIST(O)
#undef O
	__SYSCALL_COUNT
};

__END_DECLS

#endif
