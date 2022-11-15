#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/tty.h>

#include "vga.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static size_t VGA_WIDTH;
static size_t VGA_HEIGHT;
static uint16_t* VGA_MEMORY;

static size_t terminal_row;
static size_t terminal_col;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_clear()
{
	for (size_t y = 0; y < VGA_HEIGHT; y++)
		for (size_t x = 0; x < VGA_WIDTH; x++)
			terminal_putentryat(' ', terminal_color, x, y);
}

void terminal_initialize()
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
	terminal_clear();

	if (s_multiboot_info->flags & (1 << 12))
		if (s_multiboot_info->framebuffer.type != 2)
			Kernel::panic("Invalid framebuffer_type in multiboot info");
}

void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

void terminal_scroll_line(size_t line)
{
	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		const size_t index = line * VGA_WIDTH + x;
		terminal_buffer[index - VGA_WIDTH] = terminal_buffer[index];
	}
}

void terminal_clear_line(size_t line)
{
	for (size_t x = 0; x < VGA_WIDTH; x++)
		terminal_putentryat(' ', terminal_color, x, line);
}


static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}
static void terminal_update_cursor()
{
	uint16_t pos = terminal_row * VGA_WIDTH + terminal_col;
 
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void terminal_set_cursor_pos(int x, int y)
{
	terminal_row = y;
	terminal_col = x;
	terminal_update_cursor();
}

void terminal_putchar(char c)
{
	if (c == '\n')
	{
		terminal_col = 0;
		terminal_row++;
	}
	else
	{
		terminal_putentryat(c, terminal_color, terminal_col, terminal_row);
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
			terminal_scroll_line(line);
		terminal_clear_line(VGA_HEIGHT - 1);

		terminal_col = 0;
		terminal_row = VGA_HEIGHT - 1;
	}

	terminal_update_cursor();
}

void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}