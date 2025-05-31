#ifndef _AIO_H
#define _AIO_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/aio.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define __need_off_t
#define __need_size_t
#define __need_ssize_t
#include <sys/types.h>

#include <bits/types/pthread_attr_t.h>

struct aiocb
{
	int				aio_fildes;		/* File descriptor. */
	off_t			aio_offset;		/* File offset. */
	volatile void*	aio_buf;		/* Location of buffer. */
	size_t			aio_nbytes;		/* Length of transfer. */
	int				aio_reqprio;	/* Request priority offset. */
	struct sigevent	aio_sigevent;	/* Signal number and value. */
	int				aio_lio_opcode;	/* Operation to be performed. */
};

#define AIO_ALLDONE			1
#define AIO_CANCELLED		2
#define AIO_NOTCANCELLED	3
#define LIO_NOP				4
#define LIO_NOWAIT			5
#define LIO_READ			6
#define LIO_WAIT			7
#define LIO_WRITE			8

int		aio_cancel(int fildes, struct aiocb* aiocbp);
int		aio_error(const struct aiocb* aiocbp);
int		aio_fsync(int op, struct aiocb* aiocbp);
int		aio_read(struct aiocb* aiocbp);
ssize_t	aio_return(struct aiocb* aiocbp);
int		aio_suspend(const struct aiocb* const list[], int nent, const struct timespec* timeout);
int		aio_write(struct aiocb* aiocbp);
int		lio_listio(int mode, struct aiocb* __restrict const list[__restrict], int nent, struct sigevent* __restrict sig);

__END_DECLS

#endif
