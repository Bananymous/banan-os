#include <BAN/Debug.h>

#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>

#define TODO_FUNC(name, ...) int name(__VA_ARGS__) { dwarnln("TODO: " #name); errno = ENOTSUP; return -1; }

static int do_posix_spawn(pid_t* __restrict pid, const char* __restrict path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argv[], char* const envp[], bool do_path_resolution)
{
	if (file_actions != nullptr)
	{
		dwarnln("TODO: posix_spawn with file actions");
		errno = ENOTSUP;
		return -1;
	}

	if (attrp != nullptr)
	{
		dwarnln("TODO: posix_spawn with attributes");
		errno = ENOTSUP;
		return -1;
	}

	const pid_t child_pid = fork();
	if (child_pid == 0)
	{
		auto* func = do_path_resolution ? execvpe : execve;
		func(path, argv, envp);
		exit(128 + errno);
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

TODO_FUNC(posix_spawn_file_actions_addclose, posix_spawn_file_actions_t*, int)
TODO_FUNC(posix_spawn_file_actions_adddup2, posix_spawn_file_actions_t*, int, int)
TODO_FUNC(posix_spawn_file_actions_addopen, posix_spawn_file_actions_t* __restrict, int, const char* __restrict, int, mode_t)
TODO_FUNC(posix_spawn_file_actions_destroy, posix_spawn_file_actions_t*)
TODO_FUNC(posix_spawn_file_actions_init, posix_spawn_file_actions_t*)
TODO_FUNC(posix_spawnattr_destroy, posix_spawnattr_t*)
TODO_FUNC(posix_spawnattr_getflags, const posix_spawnattr_t* __restrict, short* __restrict)
TODO_FUNC(posix_spawnattr_getpgroup, const posix_spawnattr_t* __restrict, pid_t* __restrict)
TODO_FUNC(posix_spawnattr_getschedparam, const posix_spawnattr_t* __restrict, struct sched_param* __restrict)
TODO_FUNC(posix_spawnattr_getschedpolicy, const posix_spawnattr_t* __restrict, int* __restrict)
TODO_FUNC(posix_spawnattr_getsigdefault, const posix_spawnattr_t* __restrict, sigset_t* __restrict)
TODO_FUNC(posix_spawnattr_getsigmask, const posix_spawnattr_t* __restrict, sigset_t* __restrict)
TODO_FUNC(posix_spawnattr_init, posix_spawnattr_t*)
TODO_FUNC(posix_spawnattr_setflags, posix_spawnattr_t*, short)
TODO_FUNC(posix_spawnattr_setpgroup, posix_spawnattr_t*, pid_t)
TODO_FUNC(posix_spawnattr_setschedparam, posix_spawnattr_t* __restrict, const struct sched_param* __restrict)
TODO_FUNC(posix_spawnattr_setschedpolicy, posix_spawnattr_t*, int)
TODO_FUNC(posix_spawnattr_setsigdefault, posix_spawnattr_t* __restrict, const sigset_t* __restrict)
TODO_FUNC(posix_spawnattr_setsigmask, posix_spawnattr_t* __restrict, const sigset_t* __restrict)
