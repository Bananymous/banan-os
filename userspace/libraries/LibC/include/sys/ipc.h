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

#define IPC_CREAT	0x01
#define IPC_EXCL	0x02
#define IPC_NOWAIT	0x04
#define IPC_PRIVATE	0x08
#define IPC_RMID	0x10
#define IPC_SET		0x20
#define IPC_STAT	0x40

key_t ftok(const char* path, int id);

__END_DECLS

#endif
