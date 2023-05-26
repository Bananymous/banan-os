#ifndef _PWD_H
#define _PWD_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pwd.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_gid_t
#define __need_uid_t
#define __need_size_t
#include <sys/types.h>

struct passwd
{
	char*	pw_name;	/* User's login name. */
	uid_t	pw_uid;		/* Numerical user ID. */
	gid_t	pw_gid;		/* Numerical group ID. */
	char*	pw_dir;		/* Initial working directory. */
	char*	pw_shell;	/* Program to use as shell. */
};

void			endpwent(void);
struct passwd*	getpwent(void);
struct passwd*	getpwnam(const char* name);
int				getpwnam_r(const char* name, struct passwd* pwd, char* buffer, size_t bufsize, struct passwd** result);
struct passwd*	getpwuid(uid_t uid);
int				getpwuid_r(uid_t uid, struct passwd* pwd, char* buffer, size_t bufsize, struct passwd** result);
void			setpwent(void);

__END_DECLS

#endif
