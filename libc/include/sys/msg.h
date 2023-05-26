#ifndef _SYS_MSG_H
#define _SYS_MSG_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_msg.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#define __need_size_t
#define __need_ssize_t
#define __need_time_t
#include <sys/types.h>

#include <sys/ipc.h>

typedef unsigned int msgqnum_t;
typedef unsigned int msglen_t;

#define MSG_NOERROR 0

struct msqid_ds
{
	struct ipc_perm	msg_perm;	/* Operation permission structure. */
	msgqnum_t		msg_qnum;	/* Number of messages currently on queue. */
	msglen_t		msg_qbytes;	/* Maximum number of bytes allowed on queue. */
	pid_t			msg_lspid;	/* Process ID of last msgsnd(). */
	pid_t			msg_lrpid;	/* Process ID of last msgrcv(). */
	time_t			msg_stime;	/* Time of last msgsnd(). */
	time_t			msg_rtime;	/* Time of last msgrcv(). */
	time_t			msg_ctime;	/* Time of last change. */
};

int			msgctl(int msqid, int cmd, struct msqid_ds* buf);
int			msgget(key_t key, int msgflg);
ssize_t		msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg);
int			msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg);

__END_DECLS

#endif
