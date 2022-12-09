#pragma once

#include <stddef.h>

namespace TTY
{

	void initialize();
	void putchar(char c);
	void write(const char* data, size_t size);
	void writestring(const char* data);
	void set_cursor_pos(int x, int y);

}