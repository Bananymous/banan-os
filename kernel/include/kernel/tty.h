#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);

#ifdef __cplusplus
}
#endif