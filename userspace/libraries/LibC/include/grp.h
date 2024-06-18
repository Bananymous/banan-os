#ifndef _GRP_H
#define _GRP_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/grp.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_gid_t
#define __need_size_t
#include <sys/types.h>

struct group
{
	char* gr_name;	/* The name of the group. */
	gid_t gr_gid;	/* Numerical group ID. */
	char** gr_mem;	/* Pointer to a null-terminated array of character pointers to member names. */
};

void			endgrent(void);
struct group*	getgrent(void);
struct group*	getgrgid(gid_t gid);
int				getgrgit_r(gid_t gid, struct group* grp, char* buffer, size_t bufsize, struct group** result);
struct group*	getgrnam(const char* name);
int				getgrnam_r(const char* name, struct group* grp, char* buffer, size_t bufsize, struct group** result);
void			setgrent(void);

__END_DECLS

#endif
