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
	static void*	s_buffer	= nullptr;
	static void*	s_addr		= nullptr;
	static uint8_t	s_bpp		= 0;
	static uint32_t s_pitch		= 0;
	static uint32_t	s_width		= 0;
	static uint32_t	s_height	= 0;
	static uint8_t	s_mode		= 0;

	static void GraphicsPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsClear(Color color);
	static void GraphicsScroll();

	static void TextPutCharAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void TextClear(Color color);
	static void TextScroll();

	void PutEntryAt(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
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

	void Scroll()
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
			return GraphicsScroll();
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
			return TextScroll();
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

	bool PreInitialize()
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

	void Initialize()
	{
		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
		{
			s_buffer = kmalloc_eternal(s_height * s_pitch);
			if (s_buffer == nullptr)
				kprintln("Could not allocate a buffer for VESA");
			else
				memcpy(s_buffer, s_addr, s_height * s_pitch);
		}
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
		
		if (s_buffer)
		{
			uint32_t* buffer = (uint32_t*)((uint32_t)s_buffer + offset);
			switch (s_bpp)
			{
				case 24:
					*buffer = (*buffer & 0xFF000000) | (color & 0x00FFFFFF);
					*address = *buffer;
					break;
				case 32:
					*buffer = color;
					*address = color;
					break;
			}
		}
		else
		{
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
			uint32_t bytes_per_row = s_pitch / (s_bpp / 8);
			for (uint32_t y = 0; y < s_height; y++)
				for (uint32_t x = 0; x < s_width; x++)
					((uint32_t*)s_addr)[y * bytes_per_row + x] = u32_color;
			if (s_buffer)
				for (uint32_t y = 0; y < s_height; y++)
					for (uint32_t x = 0; x < s_width; x++)
						((uint32_t*)s_buffer)[y * bytes_per_row + x] = u32_color;
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

	static void GraphicsScroll()
	{
		if (s_bpp == 32)
		{
			uint32_t bytes_per_row = s_pitch / (s_bpp / 8);
			for (uint32_t y = 0; y < s_height - font.Height; y++)
			{
				for (uint32_t x = 0; x < s_width; x++)
				{
					if (s_buffer)
					{
						((uint32_t*)s_buffer)[y * bytes_per_row + x] = ((uint32_t*)s_buffer)[(y + font.Height) * bytes_per_row + x];
						((uint32_t*)s_addr  )[y * bytes_per_row + x] = ((uint32_t*)s_buffer)[(y + font.Height) * bytes_per_row + x];
					}
					else
					{
						((uint32_t*)s_addr  )[y * bytes_per_row + x] = ((uint32_t*)s_addr  )[(y + font.Height) * bytes_per_row + x];
					}
				}
			}
			return;
		}

		uint32_t row_offset_out = 0;
		uint32_t row_offset_in  = font.Height * s_pitch;

		for (uint32_t y = 0; y < s_height - 1; y++)
		{
			if (s_buffer)
			{
				memcpy((void*)((uint32_t)s_buffer + row_offset_out), (void*)((uint32_t)s_buffer + row_offset_in), s_width * s_bpp);
				memcpy((void*)((uint32_t)s_addr   + row_offset_out), (void*)((uint32_t)s_buffer + row_offset_in), s_width * s_bpp);
			}
			else
			{
				memcpy((void*)((uint32_t)s_addr   + row_offset_out), (void*)((uint32_t)s_addr + row_offset_in), s_width * s_bpp);
			}
			row_offset_out += s_pitch;
			row_offset_in  += s_pitch;
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

	static void TextScroll()
	{
		for (uint32_t y = 1; y < s_height; y++)
		{
			for (uint32_t x = 0; x < s_width; x++)
			{
				uint32_t index1 = (y - 0) * s_width + x;
				uint32_t index2 = (y - 1) * s_width + x;
				((uint16_t*)s_addr)[index2] = ((uint16_t*)s_addr)[index1];
			}
		}
	}

}