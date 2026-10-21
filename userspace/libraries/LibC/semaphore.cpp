#include <BAN/Atomic.h>

#include <assert.h>
#include <errno.h>
#include <semaphore.h>
#include <sched.h>
#include <stdarg.h>
#include <string.h>
#include <sys/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>

static constexpr char s_sem_path_base[] = "/tmp/.sem";
static constexpr size_t s_sem_path_size = sizeof(s_sem_path_base) + NAME_MAX;

static bool validate_semaphore_name(char* path, const char* name)
{
	if (name == nullptr || name[0] != '/')
	{
		errno = EINVAL;
		return false;
	}

	if (strnlen(name, NAME_MAX) >= NAME_MAX)
	{
		errno = ENAMETOOLONG;
		return false;
	}

	if (strchr(name + 1, '/'))
	{
		errno = EINVAL;
		return false;
	}

	stpcpy(stpcpy(path, s_sem_path_base), name);

	return true;
}

sem_t* sem_open(const char* name, int oflag, ...)
{
	char path[s_sem_path_size];
	if (!validate_semaphore_name(path, name))
		return nullptr;

	const bool creat      = (oflag & O_CREAT);
	const bool creat_excl = (oflag & O_EXCL ) && creat;

	int fd = -1;
	if (!creat_excl && (fd = open(path, O_RDWR)) == -1 && (!creat || errno != ENOENT))
		return nullptr;

	if (fd == -1)
	{
		assert(creat);

		va_list args;
		va_start(args, oflag);
		const auto mode  = va_arg(args, mode_t);
		const auto value = va_arg(args, unsigned);
		va_end(args);

		if (value > SEM_VALUE_MAX)
		{
			errno = EINVAL;
			return nullptr;
		}

		if (mkdir(s_sem_path_base, 0777) == -1 && errno != EEXIST)
			return nullptr;

		if ((fd = open(path, O_RDWR | O_CREAT | O_EXCL, mode)) == -1)
		{
			if (creat_excl || errno != EEXIST)
				return nullptr;
			if ((fd = open(path, O_RDWR)) == -1)
				return nullptr;
		}
		else
		{
			// TODO: proper initialization sequence to handle
			//       process dying between file creation and write
			const sem_t initial {
				.shared = 1,
				.value  = value,
			};
			if (const ssize_t ret = write(fd, &initial, sizeof(sem_t)); ret != -1)
				assert(ret == sizeof(sem_t));
			else
			{
				close(fd);
				unlink(path);
				return nullptr;
			}
		}
	}

	const auto get_ms = []() -> uint64_t {
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return ts.tv_sec / 1'000 + ts.tv_nsec / 1'000'000;
	};

	const auto timeout_ms = get_ms() + 100;

	for (;;)
	{
		struct stat st;
		if (fstat(fd, &st) == -1)
		{
			close(fd);
			return nullptr;
		}

		if (st.st_size == sizeof(sem_t))
			break;

		if (st.st_size != 0)
		{
			close(fd);
			errno = EINVAL;
			return nullptr;
		}

		if (get_ms() >= timeout_ms)
		{
			close(fd);
			errno = ETIMEDOUT;
			return nullptr;
		}

		sched_yield();
	}

	void* addr = mmap(nullptr, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);

	if (addr == MAP_FAILED)
		return nullptr;

	return static_cast<sem_t*>(addr);
}

int sem_close(sem_t* sem)
{
	return munmap(sem, sizeof(sem_t));
}

int sem_unlink(const char* name)
{
	char path[s_sem_path_size];
	if (!validate_semaphore_name(path, name))
		return -1;
	return unlink(path);
}

int sem_init(sem_t* sem, int pshared, unsigned value)
{
	if (value > SEM_VALUE_MAX)
	{
		errno = EINVAL;
		return -1;
	}

	*sem = {
		.shared = pshared,
		.value = value,
	};

	return 0;
}

int sem_destroy(sem_t* sem)
{
	(void)sem;
	return 0;
}

int sem_getvalue(sem_t* __restrict sem, int* __restrict sval)
{
	*sval = BAN::atomic_load(sem->value);
	return 0;
}

int sem_post(sem_t* sem)
{
	auto value = BAN::atomic_load(sem->value);
	for (;;)
	{
		if (value >= SEM_VALUE_MAX)
		{
			errno = EOVERFLOW;
			return -1;
		}

		if (BAN::atomic_compare_exchange(sem->value, value, value + 1))
			break;
	}

	const int op = FUTEX_WAKE | (sem->shared ? 0 : FUTEX_PRIVATE);
	futex(op, &sem->value, 1, nullptr);

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
		if (expected > 0)
		{
			if (!BAN::atomic_compare_exchange(sem->value, expected, expected - 1))
				continue;
			return 0;
		}

		const int op = FUTEX_WAIT | (sem->shared ? 0 : FUTEX_PRIVATE) | FUTEX_REALTIME;
		if (const int err = futex(op, &sem->value, expected, abstime); err && err != EAGAIN)
		{
			if (err == ETIMEDOUT)
			{
				expected = BAN::atomic_load(sem->value);
				while (expected > 0)
					if (BAN::atomic_compare_exchange(sem->value, expected, expected - 1))
						return 0;
			}

			errno = err;
			return -1;
		}
	}
}

int sem_wait(sem_t* sem)
{
	return sem_timedwait(sem, nullptr);
}
