#ifndef _SYS_FILE_H
#define _SYS_FILE_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define LOCK_UN 0
#define LOCK_EX 1
#define LOCK_SH 2
#define LOCK_NB 4

int flock(int fd, int op);

__END_DECLS

#endif
