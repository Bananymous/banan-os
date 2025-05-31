#ifndef _BITS_TYPES_PTHREAD_T_H
#define _BITS_TYPES_PTHREAD_T_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#include <sys/types.h>

typedef pid_t pthread_t;

__END_DECLS

#endif
