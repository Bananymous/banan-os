#pragma once

#include <sys/types.h>
#include <time.h>

__BEGIN_DECLS

#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec

struct stat
{
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	timespec st_atim;
	timespec st_mtim;
	timespec st_ctim;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
};

__END_DECLS