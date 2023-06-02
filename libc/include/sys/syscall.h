#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define SYS_EXIT 1
#define SYS_READ 2
#define SYS_WRITE 3
#define SYS_TERMID 4
#define SYS_CLOSE 5
#define SYS_OPEN 6
#define SYS_ALLOC 7
#define SYS_FREE 8
#define SYS_SEEK 9
#define SYS_TELL 10
#define SYS_GET_TERMIOS 11
#define SYS_SET_TERMIOS 12
#define SYS_FORK 13
#define SYS_SLEEP 14
#define SYS_EXEC 15
#define SYS_REALLOC 16

__END_DECLS

#endif
