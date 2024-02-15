#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_stat.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_blkcnt_t
#define __need_blksize_t
#define __need_dev_t
#define __need_ino_t
#define __need_mode_t
#define __need_nlink_t
#define __need_uid_t
#define __need_gid_t
#define __need_off_t
#define __need_time_t
#include <sys/types.h>

#include <time.h>

struct stat
{
	dev_t			st_dev;		/* Device ID of device containing file. */
	ino_t			st_ino;		/* File serial number. */
	mode_t			st_mode;	/* Mode of file (see below). */
	nlink_t			st_nlink;	/* Number of hard links to the file. */
	uid_t			st_uid;		/* User ID of file. */
	gid_t			st_gid;		/* Group ID of file. */
	dev_t			st_rdev;	/* Device ID (if file is character or block special). */
	off_t			st_size;	/* For regular files, the file size in bytes. For symbolic links, the length in bytes of the pathname contained in the symbolic link. For a shared memory object, the length in bytes. For a typed memory object, the length in bytes. For other file types, the use of this field is unspecified. */
	struct timespec	st_atim;	/* Last data access timestamp. */
	struct timespec	st_mtim;	/* Last data modification timestamp. */
	struct timespec	st_ctim;	/* Last file status change timestamp. */
	blksize_t		st_blksize;	/* A file system-specific preferred I/O block size for this object. In some file system types, this may vary from file to file. */
	blkcnt_t		st_blocks;	/* Number of blocks allocated for this object. */
};

#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec

#define S_IRWXU		00700
#define S_IRUSR		00400
#define S_IWUSR		00200
#define S_IXUSR		00100
#define S_IRWXG		00070
#define S_IRGRP		00040
#define S_IWGRP		00020
#define S_IXGRP		00010
#define S_IRWXO		00007
#define S_IROTH		00004
#define S_IWOTH		00002
#define S_IXOTH		00001
#define S_ISUID		04000
#define S_ISGID		02000
#define S_ISVTX		01000

#define S_IFIFO		0010000
#define S_IFCHR		0020000
#define S_IFDIR		0040000
#define S_IFBLK		0060000
#define S_IFREG		0100000
#define S_IFLNK		0120000
#define S_IFSOCK	0140000
#define S_IFMASK	0170000
#define S_IFMT		S_IFMASK

#define S_ISBLK(mode)	((mode & S_IFMASK) == S_IFBLK)
#define S_ISCHR(mode)	((mode & S_IFMASK) == S_IFCHR)
#define S_ISDIR(mode)	((mode & S_IFMASK) == S_IFDIR)
#define S_ISFIFO(mode)	((mode & S_IFMASK) == S_IFIFO)
#define S_ISREG(mode)	((mode & S_IFMASK) == S_IFREG)
#define S_ISLNK(mode)	((mode & S_IFMASK) == S_IFLNK)
#define S_ISSOCK(mode)	((mode & S_IFMASK) == S_IFSOCK)

// FIXME
#if 0
#define S_TYPEISMQ(buf)
#define S_TYPEISSEM(buf)
#define S_TYPEISSHM(buf)
#define S_TYPEISTMO(buf)
#endif

#define UTIME_NOW	1000000001
#define UTIME_OMIT	1000000002

int		chmod(const char* path, mode_t mode);
int		fchmod(int fildes, mode_t mode);
int		fchmodat(int fd, const char* path, mode_t mode, int flag);
int		fstat(int fildes, struct stat* buf);
int		fstatat(int fd, const char* __restrict path, struct stat* __restrict buf, int flag);
int		futimens(int fd, const struct timespec times[2]);
int		lstat(const char* __restrict path, struct stat* __restrict buf);
int		mkdir(const char* path, mode_t mode);
int		mkdirat(int fd, const char* path, mode_t mode);
int		mkfifo(const char* path, mode_t mode);
int		mkfifoat(int fd, const char* path, mode_t mode);
int		mknod(const char* path, mode_t mode, dev_t dev);
int		mknodat(int fd, const char* path, mode_t mode, dev_t dev);
int		stat(const char* __restrict path, struct stat* __restrict buf);
mode_t	umask(mode_t cmask);
int		utimensat(int fd, const char* path, const struct timespec times[2], int flag);

__END_DECLS

#endif
