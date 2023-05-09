#pragma once

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

__BEGIN_DECLS

int printf_impl(const char* format, va_list arguments, int (*putc_fun)(int, void*), void* data);

__END_DECLS
