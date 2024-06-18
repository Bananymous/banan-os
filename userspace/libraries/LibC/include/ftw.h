#ifndef _FTW_H
#define _FTW_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/ftw.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <sys/stat.h>

struct FTW
{
	int base;
	int level;
};

#define FTW_F	1
#define FTW_D	2
#define FTW_DNR	3
#define FTW_DP	4
#define FTW_NS	5
#define FTW_SL	6
#define FTW_SLN	7

#define FTW_PHYS	1
#define FTW_MOUNT	2
#define FTW_DEPTH	3
#define FTW_CHDIR	4

int ftw(const char* path, int (*fn)(const char*, const struct stat* ptr, int flag), int ndirs);
int nftw(const char* path, int (*fn)(const char*, const struct stat*, int, struct FTW*), int fd_limit, int flags);

__END_DECLS

#endif
