#ifndef _BITS_PRINTF_H
#define _BITS_PRINTF_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdarg.h>
#include <stddef.h>

int printf_impl(const char* format, va_list arguments, int (*putc_fun)(int, void*), void* data);

__END_DECLS

#endif
