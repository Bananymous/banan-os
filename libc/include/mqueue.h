#ifndef _MQUEUE_H
#define _MQUEUE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/mqueue.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <time.h>
#include <signal.h>

#define __need_pthread_attr_t
#define __need_size_t
#define __need_ssize_t
#include <sys/types.h>

typedef int mqd_t;

struct mq_attr
{
	long mq_flags;    /* Message queue flags. */
	long mq_maxmsg;   /* Maximum number of messages. */
	long mq_msgsize;  /* Maximum message size. */
	long mq_curmsgs;  /* Number of messages currently queued. */
};

int		mq_close(mqd_t mqdes);
int		mq_getattr(mqd_t mqdes, struct mq_attr* mqstat);
int		mq_notify(mqd_t mqdes, const struct sigevent* notification);
mqd_t	mq_open(const char* name, int oflag, ...);
ssize_t	mq_receive(mqd_t mqdes, char* msq_ptr, size_t msq_len, unsigned* msg_prio);
int		mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned msg_prio);
int		mq_setattr(mqd_t mqdes, const struct mq_attr* __restrict mqstat, struct mq_attr* __restrict omqstat);
ssize_t	mq_timedreceive(mqd_t mqdes, char* __restrict msg_ptr, size_t msg_len, unsigned* __restrict msg_prio, const struct timespec* __restrict abstime);
int		mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned msg_prio, const struct timespec* abstime);
int		mq_unlink(const char* name);

__END_DECLS

#endif
