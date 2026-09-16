#ifndef _BITS_TYPES_PTHREAD_TYPES_H
#define _BITS_TYPES_PTHREAD_TYPES_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/pthread_attr_t.h>
#include <bits/types/pthread_key_t.h>
#include <bits/types/pthread_t.h>

#include <stdint.h>

typedef uint32_t pthread_once_t;

typedef pthread_t pthread_spinlock_t;

typedef struct
{
	int type;
	int shared;
} pthread_mutexattr_t;
typedef struct
{
	pthread_mutexattr_t attr;
	uint32_t futex;
	uint32_t waiters;
	unsigned lock_depth;
} pthread_mutex_t;

typedef struct
{
	int clock;
	int shared;
} pthread_condattr_t;
typedef struct
{
	pthread_condattr_t attr;
	uint32_t futex;
} pthread_cond_t;

typedef struct
{
	int shared;
} pthread_barrierattr_t;
typedef struct
{
	pthread_mutex_t lock;
	pthread_cond_t cond;
	unsigned target;
	unsigned waiting;
	unsigned generation;
} pthread_barrier_t;

typedef struct
{
	int shared;
} pthread_rwlockattr_t;
typedef struct
{
	pthread_mutex_t lock;
	pthread_cond_t cond;
	unsigned writers_waiting;
	unsigned writer_active;
	unsigned readers_active;
} pthread_rwlock_t;

__END_DECLS

#endif
