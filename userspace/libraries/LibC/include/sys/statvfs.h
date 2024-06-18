#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_statvfs.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_fsblkcnt_t
#define __need_fsfilcnt_t
#include <sys/types.h>

struct statvfs
{
	unsigned long	f_bsize;	/* File system block size. */
	unsigned long	f_frsize;	/* Fundamental file system block size. */
	fsblkcnt_t		f_blocks;	/* Total number of blocks on file system in units of f_frsize. */
	fsblkcnt_t		f_bfree;	/* Total number of free blocks. */
	fsblkcnt_t		f_bavail;	/* Number of free blocks available to non-privileged process. */
	fsfilcnt_t		f_files;	/* Total number of file serial numbers. */
	fsfilcnt_t		f_ffree;	/* Total number of free file serial numbers. */
	fsfilcnt_t		f_favail;	/* Number of file serial numbers available to non-privileged process. */
	unsigned long	f_fsid;		/* File system ID. */
	unsigned long	f_flag;		/* Bit mask of f_flag values. */
	unsigned long	f_namemax;	/* Maximum filename length. */
};

#define ST_RDONLY 0x01
#define ST_NOSUID 0x02

int fstatvfs(int fildes, struct statvfs* buf);
int statvfs(const char* __restrict path, struct statvfs* __restrict buf);

__END_DECLS

#endif
