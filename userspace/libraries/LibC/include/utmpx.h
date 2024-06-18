#ifndef _UTMPX_H
#define _UTMPX_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/utmpx.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#include <sys/types.h>

#include <sys/time.h>

struct utmpx
{
	char			ut_user[32];	/* User login name. */
	char			ut_id[4];		/* Unspecified initialization process identifier. */
	char			ut_line[32];	/* Device name. */
	pid_t			ut_pid;			/* Process ID. */
	short			ut_type;		/* Type of entry. */
	struct timeval	ut_tv;			/* Time entry was made. */
};

#define EMPTY			0
#define BOOT_TIME		1
#define OLD_TIME		2
#define NEW_TIME		3
#define USER_PROCESS	4
#define INIT_PROCESS	5
#define LOGIN_PROCESS	6
#define DEAD_PROCESS	7

void			endutxent(void);
struct utmpx*	getutxent(void);
struct utmpx*	getutxid(const struct utmpx* id);
struct utmpx*	getutxline(const struct utmpx* line);
struct utmpx*	pututxline(const struct utmpx* utmpx);
void			setutxent(void);

__END_DECLS

#endif
