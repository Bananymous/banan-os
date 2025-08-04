#ifndef _SYS_FUTEX_H
#define _SYS_FUTEX_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>
#include <time.h>

#define FUTEX_WAIT     0
#define FUTEX_WAKE     1
#define FUTEX_PRIVATE  0x10
#define FUTEX_REALTIME 0x20

#define FUTEX_WAIT_PRIVATE (FUTEX_WAIT | FUTEX_PRIVATE)
#define FUTEX_WAKE_PRIVATE (FUTEX_WAKE | FUTEX_PRIVATE)

// op is one of FUTEX_WAIT or FUTEX_WAKE optionally or'ed with FUTEX_PRIVATE and/or FUTEX_REALTIME
//
// FUTEX_WAIT
//   put current thread to sleep until *addr != value or until timeout occurs
//   timeout is specified as a absolute time or NULL for indefinite wait
//
// FUTEX_WAKE
//   signals waiting futexes to recheck *addr. at most value threads are woken up
//
// FUTEX_PRIVATE
//   limit futex wait/wake events to the current process
//
// FUTEX_REALTIME
//   abstime corresponds to CLOCK_REALTIME instead of the default CLOCK_MONOTONIC
//
// ERRORS
//   ETIMEDOUT timeout occured
//   EINVAL    addr is not aligned on 4 byte boundary
//   ENOSYS    op contains unrecognized value
//   EINTR     function was interrupted
//   ENOMEM    not enough memory to allocate futex object
//   EAGAIN    *addr != value before thread was put to sleep
int futex(int op, const uint32_t* addr, uint32_t value, const struct timespec* abstime);

__END_DECLS

#endif
