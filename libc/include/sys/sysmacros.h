#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#include <sys/types.h>

#define makedev(maj, min) ((dev_t)(maj) << 32 | (dev_t)(min))

#define major(dev) (((dev) >> 32) & 0xFFFFFFFF)
#define minor(dev) ( (dev)        & 0xFFFFFFFF)

__END_DECLS

#endif