#include <sys/shm.h>
#include <sys/syscall.h>
#include <unistd.h>

void* shmat(int shmid, const void* shmaddr, int shmflg)
{
	const auto result = syscall(SYS_SHMAT, shmid, shmaddr, shmflg);
	if (result == -1)
		return SHM_FAILED;
	return reinterpret_cast<void*>(result);
}

int shmctl(int shmid, int cmd, struct shmid_ds* buf)
{
	return syscall(SYS_SHMCTL, shmid, cmd, buf);
}

int shmdt(const void* shmaddr)
{
	return syscall(SYS_SHMDT, shmaddr);
}

int shmget(key_t key, size_t size, int shmflg)
{
	return syscall(SYS_SHMGET, key, size, shmflg);
}
