#pragma once

#include <sys/cdefs.h>

#define ENOMEM 1
#define EINVAL 2
#define ENOTDIR 3
#define EISDIR 4
#define ENOENT 5
#define EIO 6
#define ENOTSUP 7
#define EBADF 8
#define EEXISTS 9
#define ENOTEMPTY 10
#define ENAMETOOLONG 11
#define ENOBUFS 12
#define ENOTTY 13
#define ENOTBLK 14
#define EMFILE 15
#define ENOSYS 16

#define errno errno

__BEGIN_DECLS

extern int errno;

__END_DECLS