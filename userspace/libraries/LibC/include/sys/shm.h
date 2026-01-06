#ifndef _SYS_SHM_H
#define _SYS_SHM_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_shm.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#define __need_size_t
#define __need_time_t
#include <sys/types.h>

#include <sys/ipc.h>

#include <unistd.h>

#define SHM_RDONLY 0x01
#define SHM_RND    0x02

#define SHMLBA (sysconf(_SC_PAGE_SIZE))

typedef unsigned int shmatt_t;

struct shmid_ds
{
	struct ipc_perm	shm_perm;	/* Operation permission structure. */
	size_t			shm_segsz;	/* Size of segment in bytes. */
	pid_t			shm_lpid;	/* Process ID of last shared memory operation. */
	pid_t			shm_cpid;	/* Process ID of creator. */
	shmatt_t		shm_nattch;	/* Number of current attaches. */
	time_t			shm_atime;	/* Time of last shmat(). */
	time_t			shm_dtime;	/* Time of last shmdt(). */
	time_t			shm_ctime;	/* Time of last change by shmctl().*/
};

void*	shmat(int shmid, const void* shmaddr, int shmflg);
int		shmctl(int shmid, int cmd, struct shmid_ds* buf);
int		shmdt(const void* shmaddr);
int		shmget(key_t key, size_t size, int shmflg);

__END_DECLS

#endif
