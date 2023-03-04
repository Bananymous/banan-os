#pragma once

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

__attribute__((__noreturn__))
void abort(void);

void* malloc(size_t);
void free(void*);

__END_DECLS