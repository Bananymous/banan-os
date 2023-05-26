#ifndef _PTHREAD_H
#define _PTHREAD_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <sched.h>
#include <time.h>

#define __need_size_t
#define __need_clockid_t
#define __need_pthread_attr_t
#define __need_pthread_barrier_t
#define __need_pthread_barrierattr_t
#define __need_pthread_cond_t
#define __need_pthread_condattr_t
#define __need_pthread_key_t
#define __need_pthread_mutex_t
#define __need_pthread_mutexattr_t
#define __need_pthread_once_t
#define __need_pthread_rwlock_t
#define __need_pthread_rwlockattr_t
#define __need_pthread_spinlock_t
#define __need_pthread_t
#include <sys/types.h>

#define PTHREAD_BARRIER_SERIAL_THREAD	1
#define PTHREAD_CANCEL_ASYNCHRONOUS		2
#define PTHREAD_CANCEL_ENABLE			3
#define PTHREAD_CANCEL_DEFERRED			4
#define PTHREAD_CANCEL_DISABLE			5
#define PTHREAD_CANCELED				6
#define PTHREAD_CREATE_DETACHED			7
#define PTHREAD_CREATE_JOINABLE			8
#define PTHREAD_EXPLICIT_SCHED			9
#define PTHREAD_INHERIT_SCHED			10
#define PTHREAD_MUTEX_DEFAULT			11
#define PTHREAD_MUTEX_ERRORCHECK		12
#define PTHREAD_MUTEX_NORMAL			13
#define PTHREAD_MUTEX_RECURSIVE			14
#define PTHREAD_MUTEX_ROBUST			15
#define PTHREAD_MUTEX_STALLED			16
#define PTHREAD_ONCE_INIT				17
#define PTHREAD_PRIO_INHERIT			18
#define PTHREAD_PRIO_NONE				19
#define PTHREAD_PRIO_PROTECT			20
#define PTHREAD_PROCESS_SHARED			21
#define PTHREAD_PROCESS_PRIVATE			22
#define PTHREAD_SCOPE_PROCESS			23
#define PTHREAD_SCOPE_SYSTEM			24

#if 0
#define PTHREAD_COND_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER
#endif

int			pthread_atfork(void (*prepare)(void), void (*parent)(void), void(*child)(void));
int			pthread_attr_destroy(pthread_attr_t* attr);
int			pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate);
int			pthread_attr_getguardsize(const pthread_attr_t* __restrict attr, size_t* __restrict guardsize);
int			pthread_attr_getinheritsched(const pthread_attr_t* __restrict attr, int* __restrict inheritsched);
int			pthread_attr_getschedparam(const pthread_attr_t* __restrict attr, struct sched_param* __restrict param);
int			pthread_attr_getschedpolicy(const pthread_attr_t* __restrict attr, int* __restrict policy);
int			pthread_attr_getscope(const pthread_attr_t* __restrict attr, int* __restrict contentionscope);
int			pthread_attr_getstack(const pthread_attr_t* __restrict attr, void** __restrict stackaddr, size_t* __restrict stacksize);
int			pthread_attr_getstacksize(const pthread_attr_t* __restrict attr, size_t* __restrict stacksize);
int			pthread_attr_init(pthread_attr_t* attr);
int			pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate);
int			pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize);
int			pthread_attr_setinheritsched(pthread_attr_t* attr, int inheritsched);
int			pthread_attr_setschedparam(pthread_attr_t* __restrict attr, const struct sched_param* __restrict param);
int			pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy);
int			pthread_attr_setscope(pthread_attr_t* attr, int contentionscope);
int			pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize);
int			pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);
int			pthread_barrier_destroy(pthread_barrier_t* barrier);
int			pthread_barrier_init(pthread_barrier_t* __restrict barrier, const pthread_barrierattr_t* __restrict attr, unsigned count);
int			pthread_barrier_wait(pthread_barrier_t* barrier);
int			pthread_barrierattr_destroy(pthread_barrierattr_t* attr);
int			pthread_barrierattr_getpshared( const pthread_barrierattr_t* __restrict attr, int* __restrict pshared);
int			pthread_barrierattr_init(pthread_barrierattr_t* attr);
int			pthread_barrierattr_setpshared(pthread_barrierattr_t* attr, int pshared);
int			pthread_cancel(pthread_t thread);
int			pthread_cond_broadcast(pthread_cond_t* cond);
int			pthread_cond_destroy(pthread_cond_t* cond);
int			pthread_cond_init(pthread_cond_t* __restrict cond, const pthread_condattr_t* __restrict attr);
int			pthread_cond_signal(pthread_cond_t* cond);
int			pthread_cond_timedwait(pthread_cond_t* __restrict cond, pthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime);
int			pthread_cond_wait(pthread_cond_t* __restrict cond, pthread_mutex_t* __restrict mutex);
int			pthread_condattr_destroy(pthread_condattr_t* attr);
int			pthread_condattr_getclock(const pthread_condattr_t* __restrict attr, clockid_t* __restrict clock_id);
int			pthread_condattr_getpshared(const pthread_condattr_t* __restrict attr, int* __restrict pshared);
int			pthread_condattr_init(pthread_condattr_t* attr);
int			pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id);
int			pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared);
int			pthread_create(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr, void *(*start_routine)(void*), void* __restrict arg);
int			pthread_detach(pthread_t thread);
int			pthread_equal(pthread_t t1, pthread_t t2);
void		pthread_exit(void* value_ptr);
int			pthread_getconcurrency(void);
int			pthread_getcpuclockid(pthread_t thread_id, clockid_t* clock_id);
int			pthread_getschedparam(pthread_t thread, int* __restrict policy, struct sched_param* __restrict param);
void*		pthread_getspecific(pthread_key_t key);
int			pthread_join(pthread_t thread, void** value_ptr);
int			pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int			pthread_key_delete(pthread_key_t key);
int			pthread_mutex_consistent(pthread_mutex_t* mutex);
int			pthread_mutex_destroy(pthread_mutex_t* mutex);
int			pthread_mutex_getprioceiling(const pthread_mutex_t* __restrict mutex, int* __restrict prioceiling);
int			pthread_mutex_init(pthread_mutex_t* __restrict mutex, const pthread_mutexattr_t* __restrict attr);
int			pthread_mutex_lock(pthread_mutex_t* mutex);
int			pthread_mutex_setprioceiling(pthread_mutex_t* __restrict mutex, int prioceiling, int* __restrict old_ceiling);
int			pthread_mutex_timedlock(pthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime);
int			pthread_mutex_trylock(pthread_mutex_t* mutex);
int			pthread_mutex_unlock(pthread_mutex_t* mutex);
int			pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int			pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* __restrict attr, int* __restrict prioceiling);
int			pthread_mutexattr_getprotocol(const pthread_mutexattr_t* __restrict attr, int* __restrict protocol);
int			pthread_mutexattr_getpshared(const pthread_mutexattr_t* __restrict attr, int* __restrict pshared);
int			pthread_mutexattr_getrobust(const pthread_mutexattr_t* __restrict attr, int* __restrict robust);
int			pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict attr, int* __restrict type);
int			pthread_mutexattr_init(pthread_mutexattr_t* attr);
int			pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr, int prioceiling);
int			pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr, int protocol);
int			pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared);
int			pthread_mutexattr_setrobust(pthread_mutexattr_t* attr, int robust);
int			pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int			pthread_once(pthread_once_t* once_control, void (*init_routine)(void));
int			pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int			pthread_rwlock_init(pthread_rwlock_t* __restrict rwlock, const pthread_rwlockattr_t* __restrict attr);
int			pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int			pthread_rwlock_timedrdlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime);
int			pthread_rwlock_timedwrlock(pthread_rwlock_t* __restrict rwlock, const struct timespec* __restrict abstime);
int			pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int			pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int			pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
int			pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int			pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr);
int			pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* __restrict attr, int* __restrict pshared);
int			pthread_rwlockattr_init(pthread_rwlockattr_t* attr);
int			pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared);
pthread_t	pthread_self(void);
int			pthread_setcancelstate(int state, int* oldstate);
int			pthread_setcanceltype(int type, int* oldtype);
int			pthread_setconcurrency(int new_level);
int			pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param);
int			pthread_setschedprio(pthread_t thread, int prio);
int			pthread_setspecific(pthread_key_t key, const void* value);
int			pthread_spin_destroy(pthread_spinlock_t* lock);
int			pthread_spin_init(pthread_spinlock_t* lock, int pshared);
int			pthread_spin_lock(pthread_spinlock_t* lock);
int			pthread_spin_trylock(pthread_spinlock_t* lock);
int			pthread_spin_unlock(pthread_spinlock_t* lock);
void		pthread_testcancel(void);

void		pthread_cleanup_pop(int execute);
void		pthread_cleanup_push(void (*routine)(void*), void* arg);

__END_DECLS

#endif
