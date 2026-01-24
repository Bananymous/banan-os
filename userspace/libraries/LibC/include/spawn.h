#ifndef _SPAWN_H
#define _SPAWN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/spawn.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <sched.h>
#include <signal.h>

#define __need_mode_t
#define __need_pid_t
#include <sys/types.h>

typedef struct
{
	short flags;
	pid_t pgroup;
	sched_param schedparam;
	int schedpolicy;
	sigset_t sigdefault;
	sigset_t sigmask;
} posix_spawnattr_t;

typedef struct
{
	int fildes;
} _posix_spawn_file_actions_close_t;

typedef struct
{
	int fildes;
	int newfildes;
} _posix_spawn_file_actions_dup2_t;

typedef struct
{
	int fildes;
	char* path;
	int oflag;
	mode_t mode;
} _posix_spawn_file_actions_open_t;

typedef enum
{
	_POSIX_SPAWN_FILE_ACTION_CLOSE,
	_POSIX_SPAWN_FILE_ACTION_DUP2,
	_POSIX_SPAWN_FILE_ACTION_OPEN,
} _posix_spawn_file_action_type_e;

typedef struct
{
	_posix_spawn_file_action_type_e type;
	union
	{
		_posix_spawn_file_actions_close_t close;
		_posix_spawn_file_actions_dup2_t dup2;
		_posix_spawn_file_actions_open_t open;
	};
} _posix_spawn_file_action_t;

typedef struct
{
	_posix_spawn_file_action_t* actions;
	size_t action_count;
} posix_spawn_file_actions_t;

#define POSIX_SPAWN_RESETIDS		0x01
#define POSIX_SPAWN_SETPGROUP		0x02
#define POSIX_SPAWN_SETSCHEDPARAM	0x04
#define POSIX_SPAWN_SETSCHEDULER	0x08
#define POSIX_SPAWN_SETSIGDEF		0x10
#define POSIX_SPAWN_SETSIGMASK		0x20

int posix_spawn(pid_t* __restrict pid, const char* __restrict path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argv[], char* const envp[]);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions, int fildes);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions, int fildes, int newfildes);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* __restrict file_actions, int fildes, const char* __restrict path, int oflag, mode_t mode);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions);
int posix_spawnattr_destroy(posix_spawnattr_t* attr);
int posix_spawnattr_getflags(const posix_spawnattr_t* __restrict attr, short* __restrict flags);
int posix_spawnattr_getpgroup(const posix_spawnattr_t* __restrict attr, pid_t* __restrict pgroup);
int posix_spawnattr_getschedparam(const posix_spawnattr_t* __restrict attr, struct sched_param* __restrict schedparam);
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t* __restrict attr, int* __restrict schedpolicy);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t* __restrict attr, sigset_t* __restrict sigdefault);
int posix_spawnattr_getsigmask(const posix_spawnattr_t* __restrict attr, sigset_t* __restrict sigmask);
int posix_spawnattr_init(posix_spawnattr_t* attr);
int posix_spawnattr_setflags(posix_spawnattr_t* attr, short flags);
int posix_spawnattr_setpgroup(posix_spawnattr_t* attr, pid_t pgroup);
int posix_spawnattr_setschedparam(posix_spawnattr_t* __restrict attr, const struct sched_param* __restrict schedparam);
int posix_spawnattr_setschedpolicy(posix_spawnattr_t* attr, int schedpolicy);
int posix_spawnattr_setsigdefault(posix_spawnattr_t* __restrict attr, const sigset_t* __restrict sigdefault);
int posix_spawnattr_setsigmask(posix_spawnattr_t* __restrict attr, const sigset_t* __restrict sigmask);
int posix_spawnp(pid_t* __restrict pid, const char* __restrict file, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* __restrict attrp, char* const argc[], char* const envp[]);

__END_DECLS

#endif
