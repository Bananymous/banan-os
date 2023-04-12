#pragma once

#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

__BEGIN_DECLS

[[noreturn]] void _exit(int);

pid_t fork(void);
pid_t getpid(void);

int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);

long syscall(long, ...);

__END_DECLS