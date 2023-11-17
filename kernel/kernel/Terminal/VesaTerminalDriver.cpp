#include <BAN/Errors.h>
#include <kernel/BootInfo.h>
#include <kernel/Debug.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Terminal/VesaTerminalDriver.h>

using namespace Kernel;

VesaTerminalDriver* VesaTerminalDriver::create()
{
	if (g_boot_info.framebuffer.type == FramebufferType::NONE)	
	{
		dprintln("Bootloader did not provide framebuffer");
		return nullptr;
	}

	if (g_boot_info.framebuffer.type != FramebufferType::RGB)
	{
		dprintln("unsupported framebuffer type");
		return nullptr;
	}

	if (g_boot_info.framebuffer.bpp != 24 && g_boot_info.framebuffer.bpp != 32)
	{
		dprintln("Unsupported bpp {}", g_boot_info.framebuffer.bpp);
		return nullptr;
	}

	dprintln("Graphics Mode {}x{} ({} bpp)",
		g_boot_info.framebuffer.width,
		g_boot_info.framebuffer.height,
		g_boot_info.framebuffer.bpp
	);

	paddr_t paddr = g_boot_info.framebuffer.address & PAGE_ADDR_MASK;
	size_t needed_pages = range_page_count(
		g_boot_info.framebuffer.address,
		g_boot_info.framebuffer.pitch * g_boot_info.framebuffer.height
	);

	vaddr_t vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
	ASSERT(vaddr);

	PageTable::kernel().map_range_at(paddr, vaddr, needed_pages * PAGE_SIZE, PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present);

	auto* driver = new VesaTerminalDriver(
		g_boot_info.framebuffer.width,
		g_boot_info.framebuffer.height,
		g_boot_info.framebuffer.pitch,
		g_boot_info.framebuffer.bpp,
		vaddr
	);
	driver->set_cursor_position(0, 0);
	driver->clear(TerminalColor::BLACK);
	return driver;
}

VesaTerminalDriver::~VesaTerminalDriver()
{
	PageTable::kernel().unmap_range(m_address, m_pitch * m_height);
}

void VesaTerminalDriver::set_pixel(uint32_t offset, Color color)
{	
	uint32_t* pixel = (uint32_t*)(m_address + offset);
	switch (m_bpp)
	{
		case 24:
			*pixel = (*pixel & 0xFF000000) | (color.rgb & 0x00FFFFFF);
			break;
		case 32:
			*pixel = color.rgb;
			break;
	}
}

void VesaTerminalDriver::putchar_at(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
{
	const uint8_t* glyph = font().has_glyph(ch) ? font().glyph(ch) : font().glyph('?');

	x *= font().width();
	y *= font().height();

	uint32_t row_offset = y * m_pitch + x * m_bpp / 8;
	for (uint32_t dy = 0; dy < font().height() && y + dy < m_height; dy++)
	{
		uint32_t pixel_offset = row_offset;
		for (uint32_t dx = 0; dx < font().width() && x + dx < m_width; dx++)
		{
			uint8_t bitmask = 1 << (font().width() - dx - 1);
			set_pixel(pixel_offset, glyph[dy * font().pitch()] & bitmask ? fg : bg);
			pixel_offset += m_bpp / 8;
		}
		row_offset += m_pitch;
	}
}

void VesaTerminalDriver::clear(Color color)
{
	if (m_bpp == 32)
	{
		uint32_t cells_per_row = m_pitch / 4;
		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++)
				((uint32_t*)m_address)[y * cells_per_row + x] = color.rgb;
		return;
	}

	uint32_t row_offset = 0;
	for (uint32_t y = 0; y < m_height; y++)
	{
		uint32_t pixel_offset = row_offset;
		for (uint32_t x = 0; x < m_width; x++)
		{
			set_pixel(pixel_offset, color);
			pixel_offset += m_bpp / 8;
		}
		row_offset += m_pitch;
	}
}

void VesaTerminalDriver::set_cursor_position(uint32_t x, uint32_t y)
{
	uint32_t cursor_h = font().height() / 8;
	uint32_t cursor_top = font().height() * 13 / 16;

	x *= font().width();
	y *= font().height();

	uint32_t row_offset = (y + cursor_top) * m_pitch + x * m_bpp / 8;
	for (uint32_t dy = 0; dy < cursor_h; dy++)
	{
		uint32_t pixel_offset = row_offset;
		for (uint32_t dx = 0; dx < font().width(); dx++)
		{
			set_pixel(pixel_offset, s_cursor_color);
			pixel_offset += m_bpp / 8;
		}
		row_offset += m_pitch;
	}
}