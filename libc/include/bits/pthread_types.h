// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#if !defined(__pthread_attr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_attr_t))
	#define __pthread_attr_t_defined 1
	typedef int pthread_attr_t;
#endif
#undef __need_pthread_attr_t

#if !defined(__pthread_barrier_t_defined) && (defined(__need_all_types) || defined(__need_pthread_barrier_t))
	#define __pthread_barrier_t_defined 1
	typedef int pthread_barrier_t;
#endif
#undef __need_pthread_barrier_t

#if !defined(__pthread_barrierattr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_barrierattr_t))
	#define __pthread_barrierattr_t_defined 1
	typedef int pthread_barrierattr_t;
#endif
#undef __need_pthread_barrierattr_t

#if !defined(__pthread_cond_t_defined) && (defined(__need_all_types) || defined(__need_pthread_cond_t))
	#define __pthread_cond_t_defined 1
	typedef int pthread_cond_t;
#endif
#undef __need_pthread_cond_t

#if !defined(__pthread_condattr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_condattr_t))
	#define __pthread_condattr_t_defined 1
	typedef int pthread_condattr_t;
#endif
#undef __need_pthread_condattr_t

#if !defined(__pthread_key_t_defined) && (defined(__need_all_types) || defined(__need_pthread_key_t))
	#define __pthread_key_t_defined 1
	typedef int pthread_key_t;
#endif
#undef __need_pthread_key_t

#if !defined(__pthread_mutex_t_defined) && (defined(__need_all_types) || defined(__need_pthread_mutex_t))
	#define __pthread_mutex_t_defined 1
	typedef int pthread_mutex_t;
#endif
#undef __need_pthread_mutex_t

#if !defined(__pthread_mutexattr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_mutexattr_t))
	#define __pthread_mutexattr_t_defined 1
	typedef int pthread_mutexattr_t;
#endif
#undef __need_pthread_mutexattr_t

#if !defined(__pthread_once_t_defined) && (defined(__need_all_types) || defined(__need_pthread_once_t))
	#define __pthread_once_t_defined 1
	typedef int pthread_once_t;
#endif
#undef __need_pthread_once_t

#if !defined(__pthread_rwlock_t_defined) && (defined(__need_all_types) || defined(__need_pthread_rwlock_t))
	#define __pthread_rwlock_t_defined 1
	typedef int pthread_rwlock_t;
#endif
#undef __need_pthread_rwlock_t

#if !defined(__pthread_rwlockattr_t_defined) && (defined(__need_all_types) || defined(__need_pthread_rwlockattr_t))
	#define __pthread_rwlockattr_t_defined 1
	typedef int pthread_rwlockattr_t;
#endif
#undef __need_pthread_rwlockattr_t

#if !defined(__pthread_spinlock_t_defined) && (defined(__need_all_types) || defined(__need_pthread_spinlock_t))
	#define __pthread_spinlock_t_defined 1
	typedef int pthread_spinlock_t;
#endif
#undef __need_pthread_spinlock_t

#if !defined(__pthread_t_defined) && (defined(__need_all_types) || defined(__need_pthread_t))
	#define __pthread_t_defined 1
	typedef int pthread_t;
#endif
#undef __need_pthread_t

__END_DECLS
