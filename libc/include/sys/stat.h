#pragma once

#include <sys/types.h>
#include <time.h>

#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec

__BEGIN_DECLS

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

int		chmod(const char*, mode_t);
int		fchmod(int, mode_t);
int		fchmodat(int, const char*, mode_t, int);
int		fstat(int, struct stat*);
int		fstatat(int, const char*, struct stat*, int);
int		futimens(int, const struct timespec[2]);
int		lstat(const char*, struct stat*);
int		mkdir(const char*, mode_t);
int		mkdirat(int, const char*, mode_t);
int		mkfifo(const char*, mode_t);
int		mkfifoat(int, const char*, mode_t);
int		mknod(const char*, mode_t, dev_t);
int		mknodat(int, const char*, mode_t, dev_t);
int		stat(const char*, struct stat*);
mode_t	umask(mode_t);
int		utimensat(int, const char*, const struct timespec[2], int);

__END_DECLS
