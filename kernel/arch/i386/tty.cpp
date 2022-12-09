#include <kernel/IO.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/tty.h>

#include "vga.h"

#include <stdint.h>
#include <string.h>

namespace TTY
{

	static size_t VGA_WIDTH;
	static size_t VGA_HEIGHT;
	static uint16_t* VGA_MEMORY;

	static size_t terminal_row;
	static size_t terminal_col;
	static uint8_t terminal_color;
	static uint16_t* terminal_buffer;

	void putentryat(unsigned char c, uint8_t color, size_t x, size_t y)
	{
		const size_t index = y * VGA_WIDTH + x;
		terminal_buffer[index] = vga_entry(c, color);
	}

	void clear()
	{
		for (size_t y = 0; y < VGA_HEIGHT; y++)
			for (size_t x = 0; x < VGA_WIDTH; x++)
				putentryat(' ', terminal_color, x, y);
	}

	void initialize()
	{
		if (s_multiboot_info->flags & (1 << 12))
		{
			const framebuffer_info_t& fb = s_multiboot_info->framebuffer;
			VGA_WIDTH	= fb.width;
			VGA_HEIGHT 	= fb.height;
			VGA_MEMORY	= (uint16_t*)fb.addr;
		}
		else
		{
			VGA_WIDTH	= 80;
			VGA_HEIGHT	= 25;
			VGA_MEMORY	= (uint16_t*)0xB8000;
		}

		terminal_row = 0;
		terminal_col = 0;
		terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
		terminal_buffer = VGA_MEMORY;
		clear();

		if (s_multiboot_info->flags & (1 << 12))
			if (s_multiboot_info->framebuffer.type != 2)
				Kernel::panic("Invalid framebuffer_type in multiboot info");
	}

	void setcolor(uint8_t color)
	{
		terminal_color = color;
	}

	void scroll_line(size_t line)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			const size_t index = line * VGA_WIDTH + x;
			terminal_buffer[index - VGA_WIDTH] = terminal_buffer[index];
		}
	}

	void clear_line(size_t line)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
			putentryat(' ', terminal_color, x, line);
	}

	static void update_cursor()
	{
		uint16_t pos = terminal_row * VGA_WIDTH + terminal_col;
		IO::outb(0x3D4, 0x0F);
		IO::outb(0x3D5, (uint8_t) (pos & 0xFF));
		IO::outb(0x3D4, 0x0E);
		IO::outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
	}

	void set_cursor_pos(int x, int y)
	{
		terminal_row = y;
		terminal_col = x;
		update_cursor();
	}

	void putchar(char c)
	{
		if (c == '\t')
			c = ' ';

		if (c == '\n')
		{
			terminal_col = 0;
			terminal_row++;
		}
		else if (c == '\b')
		{
			if (terminal_col > 0)
				terminal_col--;
			putentryat(' ', terminal_color, terminal_col, terminal_row);
		}
		else
		{
			putentryat(c, terminal_color, terminal_col, terminal_row);
			terminal_col++;
		}

		if (terminal_col == VGA_WIDTH)
		{
			terminal_col = 0;
			terminal_row++;
		}

		if (terminal_row == VGA_HEIGHT)
		{
			for (size_t line = 1; line < VGA_HEIGHT; line++)
				scroll_line(line);
			clear_line(VGA_HEIGHT - 1);

			terminal_col = 0;
			terminal_row = VGA_HEIGHT - 1;
		}

		update_cursor();
	}

	void write(const char* data, size_t size)
	{
		for (size_t i = 0; i < size; i++)
			putchar(data[i]);
	}

	void writestring(const char* data)
	{
		while (*data)
		{
			putchar(*data);
			data++;
		}
	}

}