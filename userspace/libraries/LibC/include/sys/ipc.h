#ifndef _SYS_IPC_H
#define _SYS_IPC_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_ipc.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_uid_t
#define __need_gid_t
#define __need_mode_t
#define __need_key_t
#include <sys/types.h>

struct ipc_perm
{
	uid_t	uid;	/* Owner's user ID. */
	gid_t	gid;	/* Owner's group ID. */
	uid_t	cuid;	/* Creator's user ID. */
	gid_t	cgid;	/* Creator's group ID. */
	mode_t	mode;	/* Read/write permission. */
};

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000

#define IPC_PRIVATE 0

#define IPC_RMID 1
#define IPC_SET  2
#define IPC_STAT 3

key_t ftok(const char* path, int id);

__END_DECLS

#endif
