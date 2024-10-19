#pragma once

#include <stdarg.h>

int scanf_impl(const char* format, va_list arguments, int (*getc_fun)(bool advance, void*), void* data);
