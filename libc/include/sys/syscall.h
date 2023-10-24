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
#define SYS_OPENAT 7
#define SYS_SEEK 11
#define SYS_TELL 12
#define SYS_GET_TERMIOS 13
#define SYS_SET_TERMIOS 14
#define SYS_FORK 15
#define SYS_EXEC 16
#define SYS_SLEEP 17
#define SYS_WAIT 18
#define SYS_FSTAT 19
#define SYS_READ_DIR_ENTRIES 21
#define SYS_SET_UID 22
#define SYS_SET_GID 23
#define SYS_SET_EUID 24
#define SYS_SET_EGID 25
#define SYS_SET_REUID 26
#define SYS_SET_REGID 27
#define SYS_GET_UID 28
#define SYS_GET_GID 29
#define SYS_GET_EUID 30
#define SYS_GET_EGID 31
#define SYS_GET_PWD 32
#define SYS_SET_PWD 33
#define SYS_CLOCK_GETTIME 34
#define SYS_PIPE 35
#define SYS_DUP 36
#define SYS_DUP2 37
#define SYS_RAISE 38
#define SYS_KILL 39
#define SYS_SIGNAL 40
#define SYS_SIGNAL_DONE 41
#define SYS_TCSETPGRP 42
#define SYS_GET_PID 43
#define SYS_GET_PGID 44
#define SYS_SET_PGID 45
#define SYS_FCNTL 46
#define SYS_NANOSLEEP 47
#define SYS_FSTATAT 48
#define SYS_STAT 49 // stat/lstat
#define SYS_SYNC 50
#define SYS_MMAP 51
#define SYS_MUNMAP 52
#define SYS_TTY_CTRL 53
#define SYS_POWEROFF 54
#define SYS_CHMOD 55

__END_DECLS

#endif
