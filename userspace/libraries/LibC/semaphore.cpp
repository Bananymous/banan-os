#include <BAN/Debug.h>

#include <errno.h>
#include <semaphore.h>

int sem_destroy(sem_t* sem)
{
	(void)sem;
	dwarnln("TODO: sem_destroy");
	errno = ENOTSUP;
	return -1;
}

int sem_init(sem_t* sem, int pshared, unsigned value)
{
	(void)sem;
	(void)pshared;
	(void)value;
	dwarnln("TODO: sem_init");
	errno = ENOTSUP;
	return -1;
}

int sem_post(sem_t* sem)
{
	(void)sem;
	dwarnln("TODO: sem_post");
	errno = ENOTSUP;
	return -1;

}

int sem_trywait(sem_t* sem)
{
	(void)sem;
	dwarnln("TODO: sem_trywait");
	errno = ENOTSUP;
	return -1;
}

int sem_wait(sem_t* sem)
{
	(void)sem;
	dwarnln("TODO: sem_wait");
	errno = ENOTSUP;
	return -1;
}
