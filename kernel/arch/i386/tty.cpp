#include <kernel/IO.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/Serial.h>
#include <kernel/tty.h>

#include "vga.h"

#include <stdint.h>
#include <string.h>

#define BEL	0x07
#define BS	0x08
#define HT	0x09
#define LF	0x0A
#define FF	0x0C
#define CR	0x0D
#define ESC	0x1B

#define CSI '['

namespace TTY
{

	static size_t VGA_WIDTH;
	static size_t VGA_HEIGHT;
	static uint16_t* VGA_MEMORY;

	static size_t terminal_row;
	static size_t terminal_col;
	static uint8_t terminal_color;
	static uint16_t* terminal_buffer;

	static char s_ansi_escape_mode		= '\0';
	static int s_ansi_escape_index		= 0;
	static int s_ansi_escape_nums[2]	= { -1, -1 };


	inline constexpr int max(int a, int b) { return a > b ? a : b; }
	inline constexpr int min(int a, int b) { return a < b ? a : b; }
	inline constexpr int clamp(int x, int a, int b) { return x < a ? a : x > b ? b : x; }

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

		for (int i = 0; i < 16; i++)
		{
			terminal_color = vga_entry_color((vga_color)i, VGA_COLOR_BLACK);
			putchar('#');
		}
		putchar('\n');
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

	static void reset_ansi_escape()
	{
		s_ansi_escape_mode = '\0';
		s_ansi_escape_index = 0;
		s_ansi_escape_nums[0] = -1;
		s_ansi_escape_nums[1] = -1;
	}

	static void handle_ansi_SGR()
	{
		switch (s_ansi_escape_nums[0])
		{
			case -1: case 0:
				terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
				break;

			case 30:
				terminal_color = vga_set_foreground(VGA_COLOR_BLACK, terminal_color);
				break;
			case 31:
				terminal_color = vga_set_foreground(VGA_COLOR_RED, terminal_color);
				break;
			case 32:
				terminal_color = vga_set_foreground(VGA_COLOR_GREEN, terminal_color);
				break;
			case 33:
				terminal_color = vga_set_foreground(VGA_COLOR_LIGHT_BROWN, terminal_color);
				break;
			case 34:
				terminal_color = vga_set_foreground(VGA_COLOR_BLUE, terminal_color);
				break;
			case 35:
				terminal_color = vga_set_foreground(VGA_COLOR_MAGENTA, terminal_color);
				break;
			case 36:
				terminal_color = vga_set_foreground(VGA_COLOR_CYAN, terminal_color);
				break;
			case 37:
				terminal_color = vga_set_foreground(VGA_COLOR_DARK_GREY, terminal_color);
				break;

			case 40:
				terminal_color = vga_set_background(VGA_COLOR_BLACK, terminal_color);
				break;
			case 41:
				terminal_color = vga_set_background(VGA_COLOR_RED, terminal_color);
				break;
			case 42:
				terminal_color = vga_set_background(VGA_COLOR_GREEN, terminal_color);
				break;
			case 43:
				terminal_color = vga_set_background(VGA_COLOR_LIGHT_BROWN, terminal_color);
				break;
			case 44:
				terminal_color = vga_set_background(VGA_COLOR_BLUE, terminal_color);
				break;
			case 45:
				terminal_color = vga_set_background(VGA_COLOR_MAGENTA, terminal_color);
				break;
			case 46:
				terminal_color = vga_set_background(VGA_COLOR_CYAN, terminal_color);
				break;
			case 47:
				terminal_color = vga_set_background(VGA_COLOR_DARK_GREY, terminal_color);
				break;
		}
	}

	static void handle_ansi_escape(char c)
	{
		switch (s_ansi_escape_mode)
		{
			case '\1':
			{
				if (c == CSI)
				{
					s_ansi_escape_mode = CSI;
					return;
				}
				return reset_ansi_escape();
			}

			case CSI:
			{
				switch (c)
				{
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
					{
						int& val = s_ansi_escape_nums[s_ansi_escape_index];
						val = (val == -1) ? (c - '0') : (val * 10 + c - '0');
						return;
					}
					case ';':
						s_ansi_escape_index++;
						return;
					case 'A': // Cursor Up
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_row = max(terminal_row - s_ansi_escape_nums[0], 0);
						return reset_ansi_escape();
					case 'B': // Curson Down
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_row = min(terminal_row + s_ansi_escape_nums[0], VGA_HEIGHT - 1);
						return reset_ansi_escape();
					case 'C': // Cursor Forward
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_col = min(terminal_col + s_ansi_escape_nums[0], VGA_WIDTH - 1);
						return reset_ansi_escape();
					case 'D': // Cursor Back
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_col = max(terminal_col - s_ansi_escape_nums[0], 0);
						return reset_ansi_escape();
					case 'E': // Cursor Next Line
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_row = min(terminal_row + s_ansi_escape_nums[0], VGA_HEIGHT - 1);
						terminal_col = 0;
						return reset_ansi_escape();
					case 'F': // Cursor Previous Line
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_row = max(terminal_row - s_ansi_escape_nums[0], 0);
						terminal_col = 0;
						return reset_ansi_escape();
					case 'G': // Cursor Horizontal Absolute
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						terminal_col = clamp(s_ansi_escape_nums[0] - 1, 0, VGA_WIDTH - 1);
						return reset_ansi_escape();
					case 'H': // Cursor Position
						if (s_ansi_escape_nums[0] == -1)
							s_ansi_escape_nums[0] = 1;
						if (s_ansi_escape_nums[1] == -1)
							s_ansi_escape_nums[1] = 1;
						terminal_row = clamp(s_ansi_escape_nums[0] - 1, 0, VGA_HEIGHT - 1);
						terminal_col = clamp(s_ansi_escape_nums[1] - 1, 0, VGA_WIDTH - 1);
						return reset_ansi_escape();
					case 'J': // Erase in Display
						dprintln("Unsupported ANSI CSI character J");
						return reset_ansi_escape();
					case 'K': // Erase in Line
						switch (s_ansi_escape_nums[0])
						{
							case -1: case 0:
								for (size_t i = terminal_col; i < VGA_WIDTH; i++)
									putentryat(' ', terminal_color, i, terminal_row);
								break;
							case 1:
								for (size_t i = 0; i <= terminal_col; i++)
									putentryat(' ', terminal_color, i, terminal_row);
								break;
							case 2:
								for (size_t i = 0; i < VGA_WIDTH; i++)
									putentryat(' ', terminal_color, i, terminal_row);
								break;
						}
						return reset_ansi_escape();
					case 'S': // Scroll Up
						dprintln("Unsupported ANSI CSI character S");
						return reset_ansi_escape();
					case 'T': // Scroll Down
						dprintln("Unsupported ANSI CSI character T");
						return reset_ansi_escape();
					case 'f': // Horizontal Vertical Position
						dprintln("Unsupported ANSI CSI character f");
						return reset_ansi_escape();
					case 'm':
						handle_ansi_SGR();
						return reset_ansi_escape();
					default:
						dprintln("Unsupported ANSI CSI character {}", c);
						return reset_ansi_escape();
				}
			}

			default:
				dprintln("Unsupported ANSI mode");
				return reset_ansi_escape();
		}
	}

	void putchar(char c)
	{
		if (s_ansi_escape_mode)
			return handle_ansi_escape(c);

		// https://en.wikipedia.org/wiki/ANSI_escape_code
		switch (c)
		{
			case BEL: // TODO
				break;
			case BS:
				if (terminal_col > 0)
					terminal_col--;
				break;
			case HT:
				terminal_col++;
				while (terminal_col % 8)
					terminal_col++;
				break;
			case LF:
				terminal_col = 0;
				terminal_row++;
				break;
			case FF:
				terminal_row++;
				break;
			case CR:
				terminal_col = 0;
				break;
			case ESC:
				s_ansi_escape_mode = '\1';
				break;
			default:
				putentryat(c, terminal_color, terminal_col, terminal_row);
				terminal_col++;
				break;
		}

		if (terminal_col >= VGA_WIDTH)
		{
			terminal_col = 0;
			terminal_row++;
		}

		while (terminal_row >= VGA_HEIGHT)
		{
			for (size_t line = 1; line < VGA_HEIGHT; line++)
				scroll_line(line);
			clear_line(VGA_HEIGHT - 1);

			terminal_col = 0;
			terminal_row--;
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