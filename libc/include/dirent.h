#pragma once

#include <sys/types.h>

__BEGIN_DECLS

struct DIR;

struct dirent
{
	ino_t d_ino;
	char d_name[];
};

int				alphasort(const struct dirent**, const struct dirent**);
int				closedir(DIR*);
int				dirfd(DIR*);
DIR*			fdopendir(int);
DIR*			opendir(const char*);
struct dirent*	readdir(DIR*);
int				readdir_r(DIR*, struct dirent*, struct dirent**);
void			rewinddir(DIR*);
int				scandir(const char*, struct dirent***, int (*)(const struct dirent*), int (*)(const struct dirent**, const struct dirent**));
void			seekdir(DIR*, long);
long			telldir(DIR*);

__END_DECLS