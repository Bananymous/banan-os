#ifndef _FCNTL_H
#define _FCNTL_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/fcntl.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <sys/stat.h>
#include <unistd.h>

#define __need_mode_t
#define __need_off_t
#define __need_pid_t
#include <sys/types.h>

#define F_DUPFD			1
#define F_DUPFD_CLOEXEC	2
#define F_GETFD			3
#define F_SETFD			4
#define F_GETFL			5
#define F_SETFL			6
#define F_GETLK			7
#define F_SETLK			8
#define F_SETLKW		9
#define F_GETOWN		10
#define F_SETOWN		11

#define FD_CLOEXEC 1

#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3

// NOTE: also defined in stdio.h
#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

/* bits 4-11 */
#define O_CLOEXEC	0x00010
#define O_CREAT		0x00020
#define O_DIRECTORY	0x00040
#define O_EXCL		0x00080
#define O_NOCTTY	0x00100
#define O_NOFOLLOW	0x00200
#define O_TRUNC		0x00400
#define O_TTY_INIT	0x00800

/* bits 12-16 */
#define O_APPEND	0x01000
#define O_DSYNC		0x02000
#define O_NONBLOCK	0x04000
#define O_RSYNC		0x08000
#define O_SYNC		0x10000

/* bits 0-3 */
#define O_RDONLY	0x00001
#define O_WRONLY	0x00002
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_SEARCH	0x00004
#define O_EXEC		0x00008
#define O_ACCMODE	0x0000F

/* bit 17 */
#define AT_FDCWD	0x20000

/* bit 18 */
#define AT_EACCESS	0x40000

/* bit 19 */
#define AT_SYMLINK_NOFOLLOW 0x80000

/* bit 20 */
#define AT_SYMLINK_FOLLOW 0x100000

/* bit 21 */
#define AT_REMOVEDIR 0x200000

/* bits 22-27 */
#define POSIX_FADV_DONTNEED		0x0400000
#define POSIX_FADV_NOREUSE		0x0800000
#define POSIX_FADV_NORMAL		0x1000000
#define POSIX_FADV_RANDOM		0x2000000
#define POSIX_FADV_SEQUENTIAL	0x4000000
#define POSIX_FADV_WILLNEED		0x8000000

struct flock
{
	short l_type;	/* Type of lock; F_RDLCK, F_WRLCK, F_UNLCK. */
	short l_whence;	/* Flag for starting offset. */
	off_t l_start;	/* Relative offset in bytes. */
	off_t l_len;	/* Size; if 0 then until EOF. */
	pid_t l_pid;	/* Process ID of the process holding the lock; returned with F_GETLK. */
};

int creat(const char* path, mode_t mode);
int fcntl(int fildes, int cmd, ...);
int open(const char* path, int oflag, ...);
int openat(int fd, const char* path, int oflag, ...);
int posix_fadvise(int fd, off_t offset, off_t len, int advice);
int posix_fallocate(int fd, off_t offset, off_t len);

__END_DECLS

#endif
