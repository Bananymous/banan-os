#include <kernel/BootInfo.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Terminal/TextModeTerminal.h>

namespace Kernel
{

	static constexpr TerminalDriver::Color s_palette[] {
		TerminalColor::BLACK,
		TerminalColor::BLUE,
		TerminalColor::GREEN,
		TerminalColor::CYAN,
		TerminalColor::RED,
		TerminalColor::MAGENTA,
		TerminalColor::YELLOW,
		TerminalColor::WHITE,
		TerminalColor::BRIGHT_BLACK,
		TerminalColor::BRIGHT_BLUE,
		TerminalColor::BRIGHT_GREEN,
		TerminalColor::BRIGHT_CYAN,
		TerminalColor::BRIGHT_RED,
		TerminalColor::BRIGHT_MAGENTA,
		TerminalColor::BRIGHT_YELLOW,
		TerminalColor::BRIGHT_WHITE,
	};

	static constexpr uint8_t color_to_text_mode_color(TerminalDriver::Color color)
	{
		uint32_t min_diff = BAN::numeric_limits<uint32_t>::max();
		uint8_t closest = 0;

		static_assert(sizeof(s_palette) / sizeof(*s_palette) == 16);
		for (size_t i = 0; i < 16; i++)
		{
			const auto rdiff = color.red()   - s_palette[i].red();
			const auto gdiff = color.green() - s_palette[i].green();
			const auto bdiff = color.blue()  - s_palette[i].blue();
			const uint32_t diff = rdiff*rdiff + gdiff*gdiff + bdiff*bdiff;
			if (diff >= min_diff)
				continue;
			min_diff = diff;
			closest = i;
		}

		return closest;
	}

	BAN::ErrorOr<BAN::RefPtr<TextModeTerminalDriver>> TextModeTerminalDriver::create_from_boot_info()
	{
		ASSERT(g_boot_info.framebuffer.type == FramebufferInfo::Type::Text);
		if (g_boot_info.framebuffer.bpp != 16)
			return BAN::Error::from_errno(ENOTSUP);
		auto* driver_ptr = new TextModeTerminalDriver(
			g_boot_info.framebuffer.address,
			g_boot_info.framebuffer.width,
			g_boot_info.framebuffer.height,
			g_boot_info.framebuffer.pitch
		);
		if (driver_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto driver = BAN::RefPtr<TextModeTerminalDriver>::adopt(driver_ptr);
		TRY(driver->initialize());
		return driver;
	}

	BAN::ErrorOr<void> TextModeTerminalDriver::initialize()
	{
		const size_t page_count = range_page_count(m_paddr, m_height * m_pitch);
		const vaddr_t vaddr = PageTable::kernel().reserve_free_contiguous_pages(page_count, KERNEL_OFFSET);
		if (vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);

		PageTable::kernel().map_range_at(
			m_paddr & PAGE_ADDR_MASK,
			vaddr,
			page_count * PAGE_SIZE,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			PageTable::MemoryType::WriteCombining
		);

		m_vaddr = vaddr + (m_paddr % PAGE_SIZE);

		set_cursor_position(0, 0);
		clear(TerminalColor::BLACK);

		return {};
	}

	TextModeTerminalDriver::~TextModeTerminalDriver()
	{
		if (m_vaddr == 0)
			return;
		const size_t page_count = range_page_count(m_paddr, m_height * m_pitch);
		PageTable::kernel().unmap_range(m_vaddr & PAGE_ADDR_MASK, page_count * PAGE_SIZE);
	}

	void TextModeTerminalDriver::putchar_at(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		if (x >= m_width || y >= m_height)
			return;

		if (ch >= 0x100)
			ch = '?';

		const uint8_t color =
			(color_to_text_mode_color(bg) << 4) |
			(color_to_text_mode_color(fg) << 0);
		MMIO::write16(m_vaddr + y * m_pitch + 2 * x, ch | (color << 8));
	}

	void TextModeTerminalDriver::clear(Color color)
	{
		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++)
				putchar_at(' ', x, y, TerminalColor::BRIGHT_WHITE, color);
	}

	void TextModeTerminalDriver::set_cursor_shown(bool shown)
	{
		if (shown)
		{
			IO::outb(0x3D4, 0x0A);
			IO::outb(0x3D5, (IO::inb(0x3D5) & 0xC0) | 14);

			IO::outb(0x3D4, 0x0B);
			IO::outb(0x3D5, (IO::inb(0x3D5) & 0xE0) | 15);
		}
		else
		{
			IO::outb(0x3D4, 0x0A);
			IO::outb(0x3D5, 0x20);
		}
	}

	void TextModeTerminalDriver::set_cursor_position(uint32_t x, uint32_t y)
	{
		const uint16_t pos = y * m_width + x;
		IO::outb(0x3D4, 0x0F);
		IO::outb(0x3D5, pos & 0xFF);
		IO::outb(0x3D4, 0x0E);
		IO::outb(0x3D5, pos >> 8);
	}

}
