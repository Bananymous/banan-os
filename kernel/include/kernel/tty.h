#pragma once

#include <stddef.h>
#include <sys/cdefs.h>

void terminal_initialize();
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_set_cursor_pos(int x, int y);