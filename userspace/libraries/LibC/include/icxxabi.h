#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

int __cxa_atexit(void (*func)(void*), void* data, void* dso_handle);
void __cxa_finalize(void* func);

__END_DECLS
