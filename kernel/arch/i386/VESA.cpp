#include <kernel/font.h>
#include <kernel/IO.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/Paging.h>
#include <kernel/panic.h>
#include <kernel/Serial.h>
#include <kernel/VESA.h>

#include <string.h>

#define MULTIBOOT_FLAGS_FRAMEBUFFER (1 << 12)
#define MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_TEXT 2


extern multiboot_info_t* s_multiboot_info;
extern const struct bitmap_font font;

namespace VESA
{

	static uintptr_t	s_addr		= 0;
	static uint8_t		s_bpp		= 0;
	static uint32_t 	s_pitch		= 0;
	static uint32_t		s_width		= 0;
	static uint32_t		s_height	= 0;
	static uint8_t		s_mode		= 0;

	static uint32_t s_terminal_width  = 0;
	static uint32_t s_terminal_height = 0;

	static void (*PutCharAtImpl)(uint16_t, uint32_t, uint32_t, Color, Color) = nullptr;
	static void (*ClearImpl)(Color) = nullptr;
	static void (*SetCursorPositionImpl)(uint32_t, uint32_t, Color) = nullptr;

	static void GraphicsPutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg);
	static void GraphicsPutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsClear(Color color);
	static void GraphicsSetCursorPosition(uint32_t x, uint32_t y, Color fg);

	static void TextPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void TextClear(Color color);
	static void TextSetCursorPosition(uint32_t x, uint32_t y, Color fg);

	void PutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg)
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			GraphicsPutBitmapAt(bitmap, x, y, fg);
	}
	void PutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			GraphicsPutBitmapAt(bitmap, x, y, fg, bg);
	}

	void PutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		if (x >= s_width || y >= s_height)
			return;
		PutCharAtImpl(ch, x, y, fg, bg);
	}

	void Clear(Color color)
	{
		ClearImpl(color);
	}

	void SetCursorPosition(uint32_t x, uint32_t y, Color fg)
	{
		SetCursorPositionImpl(x, y, fg);
	}

	uint32_t GetTerminalWidth()
	{
		return s_terminal_width;
	}

	uint32_t GetTerminalHeight()
	{
		return s_terminal_height;
	}

	bool Initialize()
	{
		if (!(s_multiboot_info->flags & MULTIBOOT_FLAGS_FRAMEBUFFER))
		{
			derrorln("bootloader did not provide a memory map");
			return false;
		}

		auto& framebuffer = s_multiboot_info->framebuffer;
		s_addr		= framebuffer.addr;
		s_bpp		= framebuffer.bpp;
		s_pitch		= framebuffer.pitch;
		s_width		= framebuffer.width;
		s_height	= framebuffer.height;
		s_mode		= framebuffer.type;

		Paging::MapPages(s_addr, s_pitch * s_height);

		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
		{
			if (s_bpp != 24 && s_bpp != 32)
			{
				derrorln("Unsupported bpp {}", s_bpp);
				return false;
			}

			dprintln("Graphics Mode {}x{} ({} bpp)", s_width, s_height, s_bpp);
			PutCharAtImpl = GraphicsPutCharAt;
			ClearImpl = GraphicsClear;
			SetCursorPositionImpl = GraphicsSetCursorPosition;
			s_terminal_width = s_width / font.Width;
			s_terminal_height = s_height / font.Height;
		}
		else if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
		{
			dprintln("Text Mode {}x{}", s_width, s_height);
			PutCharAtImpl = TextPutCharAt;
			ClearImpl = TextClear;
			SetCursorPositionImpl = TextSetCursorPosition;
			s_terminal_width = s_width;
			s_terminal_height = s_height;
		}
		else
		{
			derrorln("Unsupported type for VESA framebuffer");
			return false;
		}

		SetCursorPositionImpl(0, 0, Color::BRIGHT_WHITE);
		ClearImpl(Color::BLACK);
		return true;
	}

	static uint32_t s_graphics_colors[]
	{
		0x00'00'00'00,
		0x00'00'00'AA,
		0x00'00'AA'00,
		0x00'00'AA'AA,
		0x00'AA'00'00,
		0x00'AA'00'AA,
		0x00'AA'55'00,
		0x00'AA'AA'AA,
		0x00'55'55'55,
		0x00'55'55'FF,
		0x00'55'FF'55,
		0x00'55'FF'FF,
		0x00'FF'55'55,
		0x00'FF'55'FF,
		0x00'FF'FF'55,
		0x00'FF'FF'FF,
	};

	static void GraphicsSetPixel(uint32_t offset, uint32_t color)
	{
		uint32_t* address = (uint32_t*)(s_addr + offset);
		switch (s_bpp)
		{
			case 24:
				*address = (*address & 0xFF000000) | (color & 0x00FFFFFF);
				break;
			case 32:
				*address = color;
				break;
		}
	}

	static void GraphicsPutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg)
	{
		uint32_t u32_fg = s_graphics_colors[(uint8_t)fg];

		uint32_t fx = x * font.Width;
		uint32_t fy = y * font.Height;

		uint32_t row_offset = (fy * s_pitch) + (fx * (s_bpp / 8));
		for (uint32_t gy = 0; gy < font.Height; gy++)
		{
			uint32_t pixel_offset = row_offset;
			for (uint32_t gx = 0; gx < font.Width; gx++)
			{
				if (bitmap[gy] & (1 << (font.Width - gx - 1)))
					GraphicsSetPixel(pixel_offset, u32_fg);
				pixel_offset += s_bpp / 8;
			}
			row_offset += s_pitch;
		}
	}

	static void GraphicsPutBitmapAt(const uint8_t* bitmap, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		uint32_t u32_fg = s_graphics_colors[(uint8_t)fg];
		uint32_t u32_bg = s_graphics_colors[(uint8_t)bg];

		uint32_t fx = x * font.Width;
		uint32_t fy = y * font.Height;

		uint32_t row_offset = (fy * s_pitch) + (fx * (s_bpp / 8));
		for (uint32_t gy = 0; gy < font.Height; gy++)
		{
			uint32_t pixel_offset = row_offset;
			for (uint32_t gx = 0; gx < font.Width; gx++)
			{
				GraphicsSetPixel(pixel_offset, (bitmap[gy] & (1 << (font.Width - gx - 1))) ? u32_fg : u32_bg);
				pixel_offset += s_bpp / 8;
			}
			row_offset += s_pitch;
		}
	}

	static void GraphicsPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		// find correct bitmap
		uint32_t index = 0;
		for (uint32_t i = 0; i < font.Chars; i++)
		{
			if (font.Index[i] == ch)
			{
				index = i;
				break;
			}
		}

		const uint8_t* glyph = font.Bitmap + index * font.Height;

		GraphicsPutBitmapAt(glyph, x, y, fg, bg);
	}

	static void GraphicsClear(Color color)
	{
		uint32_t u32_color = s_graphics_colors[(uint8_t)color];

		if (s_bpp == 32)
		{
			uint32_t bytes_per_row = s_pitch / 4;
			for (uint32_t y = 0; y < s_height; y++)
				for (uint32_t x = 0; x < s_width; x++)
					((uint32_t*)s_addr)[y * bytes_per_row + x] = u32_color;
			return;
		}

		uint32_t row_offset = 0;
		for (uint32_t y = 0; y < s_height; y++)
		{
			uint32_t pixel_offset = row_offset;
			for (uint32_t x = 0; x < s_width; x++)
			{
				GraphicsSetPixel(pixel_offset, u32_color);
				pixel_offset += s_bpp / 8;
			}
			row_offset += s_pitch;
		}
	}

	static void GraphicsSetCursorPosition(uint32_t x, uint32_t y, Color fg)
	{
		if (font.Width == 8 && font.Height == 16)
		{
			uint8_t cursor[] = {
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				________,
				XXXXXXXX,
				XXXXXXXX,
				________,
			};

			GraphicsPutBitmapAt(cursor, x, y, fg);
		}
	}

	static inline uint8_t TextColor(Color fg, Color bg)
	{
		return (((uint8_t)bg & 0x0F) << 4) | ((uint8_t)fg & 0x0F);
	}

	static inline uint16_t TextEntry(uint8_t ch, uint8_t color)
	{
		return ((uint16_t)color << 8) | ch;
	}

	static void TextPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		uint32_t index = y * s_width + x;
		((uint16_t*)s_addr)[index] = TextEntry(ch, TextColor(fg, bg));
	}

	static void TextClear(Color color)
	{
		for (uint32_t y = 0; y < s_height; y++)
			for (uint32_t x = 0; x < s_width; x++)
				TextPutCharAt(' ', x, y, Color::BRIGHT_WHITE, color);
	}

	static void TextSetCursorPosition(uint32_t x, uint32_t y, Color)
	{
		uint16_t position = y * s_width + x;
		IO::outb(0x3D4, 0x0F);
		IO::outb(0x3D5, (uint8_t) (position & 0xFF));
		IO::outb(0x3D4, 0x0E);
		IO::outb(0x3D5, (uint8_t) ((position >> 8) & 0xFF));
	}

}