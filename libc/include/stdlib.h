#pragma once

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

[[noreturn]] void abort(void);
[[noreturn]] void exit(int);

int abs(int);

int atexit(void(*)(void));
int atoi(const char*);

char* getenv(const char*);

void* malloc(size_t);
void* calloc(size_t, size_t);
void free(void*);

__END_DECLS