#ifndef _SYS_UIO_H
#define _SYS_UIO_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_uio.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#define __need_ssize_t
#include <sys/types.h>

struct iovec
{
	void*	iov_base;	/* Base address of a memory region for input or output. */
	size_t	iov_len;	/* The size of the memory pointed to by iov_base. */
};

ssize_t readv(int fildes, const struct iovec* iov, int iovcnt);
ssize_t writev(int fildes, const struct iovec* iov, int iovcnt);

__END_DECLS

#endif
