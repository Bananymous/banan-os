#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define EFD_CLOEXEC   0x1
#define EFD_NONBLOCK  0x2
#define EFD_SEMAPHORE 0x4

int eventfd(unsigned int initval, int flags);

__END_DECLS

#endif
