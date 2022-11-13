#pragma once

#include <sys/cdefs.h>

#define EOF (-1)

__BEGIN_DECLS

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);

__END_DECLS