#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/Serial.h>
#include <kernel/VESA.h>

#include "font.h"

#include <string.h>

#define MULTIBOOT_FLAGS_FRAMEBUFFER (1 << 12)
#define MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_TEXT 2


extern multiboot_info_t* s_multiboot_info;
extern const struct bitmap_font font;

namespace VESA
{

	static void*	s_addr		= nullptr;
	static uint8_t	s_bpp		= 0;
	static uint32_t s_pitch		= 0;
	static uint32_t	s_width		= 0;
	static uint32_t	s_height	= 0;
	static uint8_t	s_mode		= 0;

	static uint32_t s_terminal_width  = 0;
	static uint32_t s_terminal_height = 0;

	static void (*PutCharAtImpl)(uint16_t, uint32_t, uint32_t, Color, Color) = nullptr;
	static void (*ClearImpl)(Color) = nullptr;

	static void GraphicsPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsClear(Color color);

	static void TextPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void TextClear(Color color);

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
			return false;
		
		auto& framebuffer = s_multiboot_info->framebuffer;
		s_addr		= (void*)framebuffer.addr;
		s_bpp		= framebuffer.bpp;
		s_pitch		= framebuffer.pitch;
		s_width		= framebuffer.width;
		s_height	= framebuffer.height;
		s_mode		= framebuffer.type;

		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
		{
			if (s_bpp != 24 && s_bpp != 32)
			{
				dprintln("Unsupported bpp {}", s_bpp);
				return false;
			}

			PutCharAtImpl = GraphicsPutCharAt;
			ClearImpl = GraphicsClear;
			s_terminal_width = s_width / font.Width;
			s_terminal_height = s_height / font.Height;
		}
		else if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
		{
			PutCharAtImpl = TextPutCharAt;
			ClearImpl = TextClear;
			s_terminal_width = s_width;
			s_terminal_height = s_height;
		}
		else
		{
			dprintln("Unsupported type for VESA framebuffer");
			return false;
		}

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
		uint32_t* address = (uint32_t*)((uint32_t)s_addr + offset);
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

		const unsigned char* glyph = font.Bitmap + index * font.Height;

		uint32_t u32_fg = s_graphics_colors[(uint8_t)fg];
		uint32_t u32_bg = s_graphics_colors[(uint8_t)bg];

		uint32_t fx = x * font.Width;
		uint32_t fy = y * font.Height;

		uint32_t row_offset = (fy * s_pitch) + (fx * (s_bpp / 8));
		for (uint32_t gy = 0; gy < font.Height; gy++)
		{
			if (fy + gy >= s_height) break;
			uint32_t pixel_offset = row_offset;
			for (uint32_t gx = 0; gx < font.Width; gx++)
			{
				if (fx + gx >= s_width) break;
				GraphicsSetPixel(pixel_offset, (glyph[gy] & (1 << (font.Width - gx - 1))) ? u32_fg : u32_bg);
				pixel_offset += s_bpp / 8;
			}
			row_offset += s_pitch;
		}
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

}