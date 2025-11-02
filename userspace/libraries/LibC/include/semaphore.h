#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/semaphore.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <fcntl.h>
#include <time.h>

typedef struct
{
	int shared;
	uint32_t value;
} sem_t;

#define SEM_FAILED ((sem_t*)0)

int		sem_close(sem_t* sem);
int		sem_destroy(sem_t* sem);
int		sem_getvalue(sem_t* __restrict sem, int* __restrict sval);
int		sem_init(sem_t* sem, int pshared, unsigned value);
sem_t*	sem_open(const char* name, int oflag, ...);
int		sem_post(sem_t* sem);
int		sem_timedwait(sem_t* __restrict sem, const struct timespec* __restrict abstime);
int		sem_trywait(sem_t* sem);
int		sem_unlink(const char* name);
int		sem_wait(sem_t* sem);

__END_DECLS

#endif
