#ifndef _SYS_SEM_H
#define _SYS_SEM_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_sem.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#define __need_size_t
#define __need_time_t
#include <sys/types.h>

#include <sys/ipc.h>

#define SEM_UNDO 1

#define GETNCNT	0
#define GETPID	1
#define GETVAL	2
#define GETALL	3
#define GETZCNT	4
#define SETVAL	5
#define SETALL	6

struct semid_ds
{
	struct ipc_perm	sem_perm;	/* Operation permission structure. */
	unsigned short	sem_nsems;	/* Number of semaphores in set. */
	time_t			sem_otime;	/* Last semop() time. */
	time_t			sem_ctime;	/* Last time changed by semctl(). */
};

// FIXME: A semaphore shall be represented by an anonymous structure, which shall include the following members:
// unsigned short	semval;		/* Semaphore value. */
// pid_t			sempid;		/* Process ID of last operation. */
// unsigned short	semncnt;	/* Number of processes waiting for semval to become greater than current value. */
// unsigned short	semzcnt;	/* Number of processes waiting for semval to become 0. */

struct sembuf
{
	unsigned short	sem_num;	/* Semaphore number. */
	short			sem_op;		/* Semaphore operation. */
	short			sem_flg;	/* Operation flags. */
};

int   semctl(int, int, int, ...);
int   semget(key_t, int, int);
int   semop(int, struct sembuf *, size_t);

__END_DECLS

#endif
