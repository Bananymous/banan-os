#ifndef _DIRENT_H
#define _DIRENT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/dirent.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_ino_t
#include <sys/types.h>

typedef struct __DIR DIR;

#define DT_UNKNOWN	0
#define DT_REG		1
#define DT_DIR		2
#define DT_CHR		3
#define DT_BLK		4
#define DT_FIFO		5
#define DT_SOCK		6
#define DT_LNK		7

struct dirent
{
	ino_t d_ino;			/* File serial number. */
	unsigned char d_type;	/* File type. One of DT_* definitions. */
	char d_name[256];		/* Filename string of entry. */
};

int				alphasort(const struct dirent** d1, const struct dirent** d2);
int				closedir(DIR* dirp);
int				dirfd(DIR* dirp);
DIR*			fdopendir(int fd);
DIR*			opendir(const char* dirname);
struct dirent*	readdir(DIR* dirp);
int				readdir_r(DIR* __restrict dirp, struct dirent* __restrict entry, struct dirent** __restrict result);
void			rewinddir(DIR* dirp);
int				scandir(const char* dir, struct dirent*** namelist, int (*sel)(const struct dirent*), int (*compar)(const struct dirent**, const struct dirent**));
void			seekdir(DIR* dirp, long loc);
long			telldir(DIR* dirp);

__END_DECLS

#endif
