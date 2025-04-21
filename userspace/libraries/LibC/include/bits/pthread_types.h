// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#if !defined(__pthread_attr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_attr_t) || defined(__need_pthread_types))
	#define __pthread_attr_t_defined 1
	typedef int pthread_attr_t;
#endif
#undef __need_pthread_attr_t

#if !defined(__pthread_t_defined) && (defined(__need_all_types) || defined(__need_pthread_t) || defined(__need_pthread_types))
	#define __pthread_t_defined 1
	typedef pid_t pthread_t;
#endif
#undef __need_pthread_t

#if !defined(__pthread_types_defined) && (defined(__need_all_types) || defined(__need_pthread_types))
#define __pthread_types_defined 1

typedef int pthread_once_t;

typedef unsigned pthread_key_t;

typedef pthread_t pthread_spinlock_t;

typedef struct { int type; int shared; } pthread_mutexattr_t;
typedef struct { pthread_mutexattr_t attr; pthread_t locker; unsigned lock_depth; } pthread_mutex_t;

typedef int pthread_barrierattr_t;
typedef int pthread_barrier_t;

typedef struct { int clock; int shared; } pthread_condattr_t;
struct _pthread_cond_block { struct _pthread_cond_block* next; int signaled; };
typedef struct { pthread_condattr_t attr; pthread_spinlock_t lock; struct _pthread_cond_block* block_list; } pthread_cond_t;

typedef struct { int shared; } pthread_rwlockattr_t;
typedef struct { pthread_rwlockattr_t attr; unsigned lockers; unsigned writers; } pthread_rwlock_t;

#endif
#undef __need_pthread_types

__END_DECLS
