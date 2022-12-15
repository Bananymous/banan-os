#include <kernel/multiboot.h>
#include <kernel/Serial.h>
#include <kernel/VESA.h>

#define MULTIBOOT_FLAGS_FRAMEBUFFER (1 << 12)
#define MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_TEXT 2

extern multiboot_info_t* s_multiboot_info;

namespace VESA
{
	static void*	s_addr		= nullptr;
	static uint8_t	s_bpp		= 0;
	static uint32_t	s_width		= 0;
	static uint32_t	s_height	= 0;
	static uint8_t	s_mode		= 0;

	static void GraphicsPutCharAt(char ch, uint32_t x, uint32_t y, Color fg, Color bg);
	static void GraphicsClear(Color color);
	static void GraphicsScrollLine(uint32_t line);
	static bool InitializeGraphicsMode();

	static inline uint8_t TextColor(Color fg, Color bg);
	static inline uint16_t TextEntry(uint8_t ch, uint8_t color);
	static void TextPutCharAt(char c, uint32_t x, uint32_t y, Color fg, Color bg);
	static void TextClear(Color color);
	static void TextScrollLine(uint32_t line);
	static bool InitializeTextMode();

	void PutEntryAt(char ch, uint32_t x, uint32_t y, Color fg, Color bg)
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

	uint32_t GetWidth()
	{
		return s_width;
	}

	uint32_t GetHeight()
	{
		return s_height;
	}

	bool Initialize()
	{
		if (!(s_multiboot_info->flags & MULTIBOOT_FLAGS_FRAMEBUFFER))
			return false;
		
		auto& framebuffer = s_multiboot_info->framebuffer;
		s_addr		= (void*)framebuffer.addr;
		s_bpp		= framebuffer.bpp;
		s_width		= framebuffer.width;
		s_height	= framebuffer.height;
		s_mode		= framebuffer.type;

		if (s_mode == MULTIBOOT_FRAMEBUFFER_TYPE_GRAPHICS)
		{
			dprintln("VESA in Graphics mode {}x{} ({} bpp)", s_width, s_height, s_bpp);
			GraphicsClear(Color::BLACK);
			return false;
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






	static void GraphicsPutCharAt(char ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{

	}

	static void GraphicsClear(Color color)
	{

	}

	static void GraphicsScrollLine(uint32_t line)
	{

	}

	static bool InitializeGraphicsMode()
	{
		return false;
	}







	static inline uint8_t TextColor(Color fg, Color bg)
	{
		return ((bg & 0x0F) << 4) | (fg & 0x0F);
	}

	static inline uint16_t TextEntry(uint8_t ch, uint8_t color)
	{
		return ((uint16_t)color << 8) | ch;
	}

	static void TextPutCharAt(char c, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		uint64_t index = y * s_width + x;
		((uint16_t*)s_addr)[index] = TextEntry(c, TextColor(fg, bg));
	}

	static void TextClear(Color color)
	{
		for (uint32_t y = 0; y < s_height; y++)
			for (uint32_t x = 0; x < s_width; x++)
				TextPutCharAt(' ', x, y, Color::WHITE, color);
	}

	static void TextScrollLine(uint32_t line)
	{
		for (uint32_t x = 0; x < s_width; x++)
		{
			uint64_t index1 = (line - 0) * s_width + x;
			uint64_t index2 = (line - 1) * s_width + x;
			((uint16_t*)s_addr)[index2] = ((uint16_t*)s_addr)[index1];
		}
	}

	static bool InitializeTextMode()
	{
		TextClear(Color::BLACK);
		return true;
	}

}