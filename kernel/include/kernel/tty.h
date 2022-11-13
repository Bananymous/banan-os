#pragma once

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void terminal_initialize();
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);

__END_DECLS