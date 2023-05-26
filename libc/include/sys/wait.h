#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_wait.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_id_t
#define __need_pid_t
#include <sys/types.h>

#include <signal.h>

#define WCONTINUED	0x01
#define WNOHANG		0x02
#define WUNTRACED	0x04
#define WEXITED		0x08
#define WNOWAIT		0x10
#define WSTOPPED	0x20

// FIXME
#if 0
#define WEXITSTATUS
#define WIFCONTINUED
#define WIFEXITED
#define WIFSIGNALED
#define WIFSTOPPED
#define WSTOPSIG
#define WTERMSIG
#endif

typedef enum
{
	P_ALL,
	P_PGID,
	P_PID,
} idtype_t;

pid_t	wait(int* stat_loc);
int		waitid(idtype_t idtype, id_t id, siginfo_t* infop, int options);
pid_t	waitpid(pid_t pid, int* stat_loc, int options);

__END_DECLS

#endif
