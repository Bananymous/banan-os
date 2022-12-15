#include <kernel/multiboot.h>
#include <kernel/Serial.h>
#include <kernel/VESA.h>

#include "font.h"

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

	static void GraphicsPutCharAt(uint8_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsClear(Color color);
	static void GraphicsScrollLine(uint32_t line);

	static void TextPutCharAt(uint8_t c, uint32_t x, uint32_t y, Color fg, Color bg);
	static void TextClear(Color color);
	static void TextScrollLine(uint32_t line);

	void PutEntryAt(uint8_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		if (x >= s_width)
			return;
		if (y >= s_height)
			return;
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return GraphicsPutCharAt(ch, x, y, fg, bg);
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return TextPutCharAt(ch, x, y, fg, bg);
	}

	void Clear(Color color)
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return GraphicsClear(color);
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return TextClear(color);
	}

	void ScrollLine(uint32_t line)
	{
		if (line == 0 || line >= s_height)
			return;
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return GraphicsScrollLine(line);
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return TextScrollLine(line);
	}

	uint32_t GetTerminalWidth()
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return s_width / font.Width;
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return s_width;
		return 0;
	}

	uint32_t GetTerminalHeight()
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return s_height / font.Height;
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return s_height;
		return 0;
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

			dprintln("VESA in Graphics mode {}x{} ({} bpp)", s_width, s_height, s_bpp);
			GraphicsClear(Color::BLACK);
			return true;
		}
		
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
		{
			dprintln("VESA in Text mode {}x{}", s_width, s_height);
			TextClear(Color::BLACK);
			return true;
		}
		
		dprintln("Unsupported type for VESA framebuffer");
		return false;
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

	static void GraphicsPutPixelAt(uint32_t offset, uint32_t color)
	{
		uint32_t* pixel = (uint32_t*)((uint8_t*)s_addr + offset);
		switch (s_bpp)
		{
			case 24:
				*pixel = (*pixel & 0xFF000000) | color;
				break;
			case 32:
				*pixel = color;
				break;
		}
	}

	static void GraphicsPutPixelAt(uint32_t x, uint32_t y, uint32_t color)
	{
		uint32_t offset = y * s_pitch + (x * (s_bpp / 8));
		GraphicsPutPixelAt(offset, color);
	}	

	static void GraphicsPutCharAt(uint8_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
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

		for (uint32_t cy = 0; cy < font.Height; cy++)
			if (y * font.Height + cy < s_height)
				for (uint32_t cx = 0; cx < font.Width; cx++)
					if (x * font.Width + cx < s_width)
						GraphicsPutPixelAt(x * font.Width + cx, y * font.Height + cy, glyph[cy] & (1 << (font.Width - cx - 1)) ? u32_fg : u32_bg);
	}

	static void GraphicsClear(Color color)
	{
		uint32_t u32_color = s_graphics_colors[(uint8_t)color];
		for (uint32_t y = 0; y < s_height; y++)
			for (uint32_t x = 0; x < s_width; x++)
				GraphicsPutPixelAt(x, y, u32_color);
	}

	static void GraphicsScrollLine(uint32_t line)
	{
		(void)line;
	}





	static inline uint8_t TextColor(Color fg, Color bg)
	{
		return (((uint8_t)bg & 0x0F) << 4) | ((uint8_t)fg & 0x0F);
	}

	static inline uint16_t TextEntry(uint8_t ch, uint8_t color)
	{
		return ((uint16_t)color << 8) | ch;
	}

	static void TextPutCharAt(uint8_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
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

	static void TextScrollLine(uint32_t line)
	{
		for (uint32_t x = 0; x < s_width; x++)
		{
			uint32_t index1 = (line - 0) * s_width + x;
			uint32_t index2 = (line - 1) * s_width + x;
			((uint16_t*)s_addr)[index2] = ((uint16_t*)s_addr)[index1];
		}
	}

}