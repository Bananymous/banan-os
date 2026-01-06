#include <BAN/Atomic.h>

#include <errno.h>
#include <semaphore.h>
#include <sys/futex.h>

int sem_destroy(sem_t* sem)
{
	(void)sem;
	return 0;
}

int sem_init(sem_t* sem, int pshared, unsigned value)
{
	*sem = {
		.shared = pshared,
		.value = value,
	};
	return 0;
}

int sem_getvalue(sem_t* __restrict sem, int* __restrict sval)
{
	*sval = BAN::atomic_load(sem->value);
	return 0;
}

int sem_post(sem_t* sem)
{
	const auto old = BAN::atomic_fetch_add(sem->value, 1);
	if (old == 0)
		futex(FUTEX_WAKE, &sem->value, 1, nullptr);
	return 0;
}

int sem_trywait(sem_t* sem)
{
	uint32_t expected = BAN::atomic_load(sem->value);
	while (expected)
		if (BAN::atomic_compare_exchange(sem->value, expected, expected - 1))
			return 0;
	errno = EAGAIN;
	return -1;
}

int sem_timedwait(sem_t* __restrict sem, const struct timespec* __restrict abstime)
{
	for (;;)
	{
		uint32_t expected = BAN::atomic_load(sem->value);
		if (expected > 0 && BAN::atomic_compare_exchange(sem->value, expected, expected - 1))
			return 0;

		const int op = FUTEX_WAIT | (sem->shared ? 0 : FUTEX_PRIVATE) | FUTEX_REALTIME;
		if (futex(op, &sem->value, expected, abstime) == -1 && (errno == EINTR || errno == ETIMEDOUT))
			return -1;
	}
}

int sem_wait(sem_t* sem)
{
	return sem_timedwait(sem, nullptr);
}
