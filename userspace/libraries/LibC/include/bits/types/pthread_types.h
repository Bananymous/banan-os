#ifndef _BITS_TYPES_PTHREAD_TYPES_H
#define _BITS_TYPES_PTHREAD_TYPES_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/pthread_attr_t.h>
#include <bits/types/pthread_t.h>

typedef int pthread_once_t;

typedef unsigned pthread_key_t;

typedef pthread_t pthread_spinlock_t;

typedef struct
{
	int type;
	int shared;
} pthread_mutexattr_t;
typedef struct
{
	pthread_mutexattr_t attr;
	pthread_t locker;
	unsigned lock_depth;
} pthread_mutex_t;

typedef struct
{
	int shared;
} pthread_barrierattr_t;
typedef struct
{
	pthread_barrierattr_t attr;
	unsigned target;
	unsigned waiting;
} pthread_barrier_t;

typedef struct
{
	int clock;
	int shared;
} pthread_condattr_t;
struct _pthread_cond_block
{
	struct _pthread_cond_block* next;
	int signaled;
};
typedef struct
{
	pthread_condattr_t attr;
	pthread_spinlock_t lock;
	struct _pthread_cond_block* block_list;
} pthread_cond_t;

typedef struct
{
	int shared;
} pthread_rwlockattr_t;
typedef struct
{
	pthread_rwlockattr_t attr;
	unsigned lockers;
	unsigned writers;
} pthread_rwlock_t;

__END_DECLS

#endif
