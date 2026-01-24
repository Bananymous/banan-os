#include <BAN/Atomic.h>

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <unistd.h>

#define TODO_FUNC(name, ...) int name(__VA_ARGS__) { dwarnln("TODO: " #name); errno = ENOTSUP; return -1; }

int posix_spawnattr_init(posix_spawnattr_t* attr)
{
	attr->flags = 0;
	return 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t* attr)
{
	(void)attr;
	return 0;
}

int posix_spawnattr_getflags(const posix_spawnattr_t* __restrict attr, short* __restrict flags)
{
	*flags = attr->flags;
	return 0;
}

int posix_spawnattr_setflags(posix_spawnattr_t* attr, short flags)
{
	if (flags & ~(POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK))
		return EINVAL;
	attr->flags = flags;
	return 0;
}

int posix_spawnattr_getpgroup(const posix_spawnattr_t* __restrict attr, pid_t* __restrict pgroup)
{
	*pgroup = attr->pgroup;
	return 0;
}

int posix_spawnattr_setpgroup(posix_spawnattr_t* attr, pid_t pgroup)
{
	attr->pgroup = pgroup;
	return 0;
}

int posix_spawnattr_getschedparam(const posix_spawnattr_t* __restrict attr, struct sched_param* __restrict schedparam)
{
	*schedparam = attr->schedparam;
	return 0;
}

int posix_spawnattr_setschedparam(posix_spawnattr_t* __restrict attr, const struct sched_param* __restrict schedparam)
{
	attr->schedparam = *schedparam;
	return 0;
}

int posix_spawnattr_getschedpolicy(const posix_spawnattr_t* __restrict attr, int* __restrict schedpolicy)
{
	*schedpolicy = attr->schedpolicy;
	return 0;
}

int posix_spawnattr_setschedpolicy(posix_spawnattr_t* attr, int schedpolicy)
{
	switch (schedpolicy)
	{
		case SCHED_FIFO:
		case SCHED_RR:
		case SCHED_SPORADIC:
		case SCHED_OTHER:
			break;
		default:
			return EINVAL;
	}

	attr->schedpolicy = schedpolicy;
	return 0;
}

int posix_spawnattr_getsigdefault(const posix_spawnattr_t* __restrict attr, sigset_t* __restrict sigdefault)
{
	*sigdefault = attr->sigdefault;
	return 0;
}

int posix_spawnattr_setsigdefault(posix_spawnattr_t* __restrict attr, const sigset_t* __restrict sigdefault)
{
	attr->sigdefault = *sigdefault;
	return 0;
}

int posix_spawnattr_getsigmask(const posix_spawnattr_t* __restrict attr, sigset_t* __restrict sigmask)
{
	*sigmask = attr->sigmask;
	return 0;
}

int posix_spawnattr_setsigmask(posix_spawnattr_t* __restrict attr, const sigset_t* __restrict sigmask)
{
	attr->sigmask = *sigmask;
	return 0;
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions)
{
	*file_actions = {
		.actions = nullptr,
		.action_count = 0,
	};
	return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions)
{
	for (size_t i = 0; i < file_actions->action_count; i++)
	{
		switch (file_actions->actions[i].type)
		{
			case _POSIX_SPAWN_FILE_ACTION_CLOSE:
			case _POSIX_SPAWN_FILE_ACTION_DUP2:
				break;
			case _POSIX_SPAWN_FILE_ACTION_OPEN:
				free(file_actions->actions[i].open.path);
				break;
		}
	}

	if (file_actions->actions != nullptr)
		free(file_actions->actions);

	file_actions->actions = nullptr;
	file_actions->action_count = 0;

	return 0;
}

static int add_file_action(posix_spawn_file_actions_t* file_actions, _posix_spawn_file_action_t action)
{
	void* new_actions = realloc(file_actions->actions, sizeof(_posix_spawn_file_action_t) * (file_actions->action_count + 1));
	if (new_actions == nullptr)
		return ENOMEM;
	file_actions->actions = static_cast<_posix_spawn_file_action_t*>(new_actions);
	file_actions->actions[file_actions->action_count++] = action;
	return 0;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions, int fildes)
{
	if (fildes < 0 || fildes >= OPEN_MAX)
		return EBADF;

	return add_file_action(file_actions, {
		.type = _POSIX_SPAWN_FILE_ACTION_CLOSE,
		.close = {
			.fildes = fildes,
		}
	});
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions, int fildes, int newfildes)
{
	if (fildes < 0 || fildes >= OPEN_MAX || newfildes < 0 || newfildes >= OPEN_MAX)
		return EBADF;

	return add_file_action(file_actions, {
		.type = _POSIX_SPAWN_FILE_ACTION_DUP2,
		.dup2 = {
			.fildes = fildes,
			.newfildes = newfildes,
		}
	});
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* __restrict file_actions, int fildes, const char* __restrict path, int oflag, mode_t mode)
{
	if (fildes < 0 || fildes >= OPEN_MAX)
		return EBADF;

	char* path_copy = strdup(path);
	if (path_copy == nullptr)
		return ENOMEM;

	const auto ret = add_file_action(file_actions, {
		.type = _POSIX_SPAWN_FILE_ACTION_OPEN,
		.open = {
			.fildes = fildes,
			.path = path_copy,
			.oflag = oflag,
			.mode = mode,
		}
	});

	if (ret != 0)
		free(path_copy);

	return ret;
}

static int do_posix_spawn(pid_t* __restrict pid, const char* __restrict path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argv[], char* const envp[], bool do_path_resolution)
{
	// MAP_SHARED | MAP_ANONYMOUS is not supported :D

	const auto smo_key = smo_create(sizeof(int), PROT_READ | PROT_WRITE);
	if (smo_key == -1)
		return errno;

	void* addr = smo_map(smo_key);
	smo_delete(smo_key);
	if (addr == MAP_FAILED)
		return errno;

	auto& child_status = *static_cast<BAN::Atomic<int>*>(addr);
	child_status = INT_MAX;

	const pid_t child_pid = fork();
	if (child_pid == 0)
	{
#define DIE_ON_ERROR(err, ...) \
			do { \
				auto ret = __VA_ARGS__; \
				if (ret != (err)) \
					break; \
				child_status = errno; \
				exit(127); \
			} while (false)

		if (attrp != nullptr)
		{
			if (attrp->flags & POSIX_SPAWN_RESETIDS)
				DIE_ON_ERROR(-1, seteuid(getuid()));

			if (attrp->flags & POSIX_SPAWN_SETPGROUP)
				DIE_ON_ERROR(-1, setpgid(0, attrp->pgroup));

			if (attrp->flags & POSIX_SPAWN_SETSCHEDULER)
				DIE_ON_ERROR(-1, (errno = ENOTSUP, -1));
			//	DIE_ON_ERROR(-1, sched_setscheduler(0, attrp->schedpolicy, &attrp->schedparam));
			else if (attrp->flags & POSIX_SPAWN_SETSCHEDPARAM)
				DIE_ON_ERROR(-1, (errno = ENOTSUP, -1));
			//	DIE_ON_ERROR(-1, sched_setparam(0, &attrp->schedparam));

			if (attrp->flags & POSIX_SPAWN_SETSIGDEF)
				for (int sig = _SIGMIN; sig <= _SIGMAX; sig++)
					if (attrp->sigdefault & (1ull << sig))
						DIE_ON_ERROR(NULL, signal(sig, SIG_DFL));

			if (attrp->flags & POSIX_SPAWN_SETSIGMASK)
				DIE_ON_ERROR(-1, sigprocmask(SIG_SETMASK, &attrp->sigmask, nullptr));
		}

		if (file_actions != nullptr)
		{
			for (size_t i = 0; i < file_actions->action_count; i++)
			{
				const auto& action = file_actions->actions[i];
				switch (action.type)
				{
					case _POSIX_SPAWN_FILE_ACTION_CLOSE:
						// EBADF is not considered an error with addclose
						close(action.close.fildes);
						break;
					case _POSIX_SPAWN_FILE_ACTION_DUP2:
						if (action.dup2.fildes != action.dup2.newfildes)
							DIE_ON_ERROR(-1, dup2(action.dup2.fildes, action.dup2.newfildes));
						else
							DIE_ON_ERROR(-1, fcntl(action.dup2.fildes, F_SETFD, fcntl(action.dup2.fildes, F_GETFD) & ~O_CLOEXEC));
						break;
					case _POSIX_SPAWN_FILE_ACTION_OPEN:
						const int fd = open(action.open.path, action.open.oflag, action.open.mode);
						DIE_ON_ERROR(-1, fd);
						if (fd != action.open.fildes)
						{
							DIE_ON_ERROR(-1, dup2(fd, action.open.fildes));
							close(fd);
						}
						break;
				}
			}
		}

#undef DIE_ON_ERROR

		child_status = 0;

		auto* func = do_path_resolution ? execvpe : execve;
		func(path, argv, envp);
		exit(127);
	}

	if (child_pid == -1)
	{
		munmap(addr, sizeof(int));
		return errno;
	}

	while (child_status == INT_MAX)
		sched_yield();

	const int child_status_copy = child_status;
	munmap(addr, sizeof(int));

	if (child_status_copy != 0)
	{
		while (waitpid(child_pid, nullptr, 0) == -1 && errno == EINTR)
			continue;
		return child_status_copy;
	}

	if (pid != nullptr)
		*pid = child_pid;

	return 0;
}

int posix_spawn(pid_t* __restrict pid, const char* __restrict path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argv[], char* const envp[])
{
	return do_posix_spawn(pid, path,file_actions, attrp, argv, envp, false);
}

int posix_spawnp(pid_t* __restrict pid, const char* __restrict path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argv[], char* const envp[])
{
	return do_posix_spawn(pid, path,file_actions, attrp, argv, envp, true);
}
