#pragma once

#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// fork(), execv(), execve(), execvp(), getpid()

__BEGIN_DECLS

pid_t fork(void);

int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);

pid_t getpid(void);

__END_DECLS