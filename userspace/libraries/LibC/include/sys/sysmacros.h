#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#include <sys/types.h>

#define makedev(maj, min) ((dev_t)(maj) << 16 | (dev_t)(min))

#define major(dev) (((dev) >> 16) & 0xFFFF)
#define minor(dev) ( (dev)        & 0xFFFF)

__END_DECLS

#endif
