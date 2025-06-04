#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Terminal/VirtualTTY.h>

#include <fcntl.h>
#include <string.h>

#define BEL	0x07
#define BS	0x08
#define HT	0x09
#define LF	0x0A
#define FF	0x0C
#define CR	0x0D
#define ESC	0x1B

#define CSI '['

namespace Kernel
{

	static BAN::Atomic<uint32_t> s_next_tty_number = 0;

	BAN::ErrorOr<BAN::RefPtr<VirtualTTY>> VirtualTTY::create(BAN::RefPtr<TerminalDriver> driver)
	{
		auto* tty_ptr = new VirtualTTY(driver);
		ASSERT(tty_ptr);

		auto tty = BAN::RefPtr<VirtualTTY>::adopt(tty_ptr);
		DevFileSystem::get().add_device(tty);
		return tty;
	}

	VirtualTTY::VirtualTTY(BAN::RefPtr<TerminalDriver> driver)
		: TTY({
			.c_iflag = 0,
			.c_oflag = 0,
			.c_cflag = CS8,
			.c_lflag = ECHO | ECHOE | ECHOK  | ICANON | ISIG,
			.c_cc = TTY_DEFAULT_TERMIOS_CC,
			.c_ospeed = B38400,
			.c_ispeed = B38400,
		  }, 0600, 0, 0)
		, m_name(MUST(BAN::String::formatted("tty{}", s_next_tty_number++)))
		, m_terminal_driver(driver)
		, m_palette(driver->palette())
		, m_foreground(m_palette[15])
		, m_background(m_palette[0])
	{
		m_width = m_terminal_driver->width();
		m_height = m_terminal_driver->height();

		m_buffer = new Cell[m_width * m_height];
		ASSERT(m_buffer);
	}

	void VirtualTTY::clear()
	{
		SpinLockGuard _(m_write_lock);
		for (uint32_t i = 0; i < m_width * m_height; i++)
			m_buffer[i] = { .foreground = m_foreground, .background = m_background, .codepoint = ' ' };
		m_terminal_driver->clear(m_background);
	}

	BAN::ErrorOr<void> VirtualTTY::set_font(LibFont::Font&& font)
	{
		if (!m_terminal_driver->has_font())
			return BAN::Error::from_errno(EINVAL);

		SpinLockGuard _(m_write_lock);

		TRY(m_terminal_driver->set_font(BAN::move(font)));

		uint32_t new_width = m_terminal_driver->width();
		uint32_t new_height = m_terminal_driver->height();

		if (m_width != new_width || m_height != new_height)
		{
			Cell* new_buffer = new Cell[new_width * new_height];
			ASSERT(new_buffer);

			for (uint32_t i = 0; i < new_width * m_height; i++)
				new_buffer[i] = { .foreground = m_foreground, .background = m_background, .codepoint = ' ' };

			for (uint32_t y = 0; y < BAN::Math::min<uint32_t>(m_height, new_height); y++)
				for (uint32_t x = 0; x < BAN::Math::min<uint32_t>(m_width, new_width); x++)
					new_buffer[y * new_width + x] = m_buffer[y * m_width + x];

			delete[] m_buffer;
			m_buffer = new_buffer;
			m_width = new_width;
			m_height = new_height;
		}

		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++)
				render_from_buffer(x, y);

		return {};
	}

	void VirtualTTY::reset_ansi()
	{
		ASSERT(m_write_lock.current_processor_has_lock());
		m_ansi_state = {
			.nums = { -1, -1, -1, -1, -1 },
			.index = 0,
			.question = false,
		};
		m_state = State::Normal;
	}

	void VirtualTTY::handle_ansi_csi_color(uint8_t ch)
	{
		ASSERT(m_write_lock.current_processor_has_lock());
		switch (ch)
		{
			case 0:
				m_foreground = m_palette[15];
				m_background = m_palette[0];
				m_colors_inverted = false;
				break;

			case 7:  m_colors_inverted = true;  break;
			case 27: m_colors_inverted = false; break;

			case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
				m_foreground = m_palette[ch - 30];
				break;

			case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
				m_background = m_palette[ch - 40];
				break;

			case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
				m_foreground = m_palette[ch - 90 + 8];
				break;

			case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
				m_background = m_palette[ch - 100 + 8];
				break;
		}
	}

	BAN::Optional<TerminalDriver::Color> VirtualTTY::get_8bit_color()
	{
		ASSERT(m_ansi_state.nums[1] == 5);
		if (m_ansi_state.nums[2] < 1)
			return {};

		const uint8_t code = BAN::Math::min(m_ansi_state.nums[2], 256) - 1;
		if (code < 16)
			return m_palette[code];

		if (code < 232)
		{
			const uint8_t r = (code - 16) / 36 % 6 * 40 + 55;
			const uint8_t g = (code - 16) /  6 % 6 * 40 + 55;
			const uint8_t b = (code - 16) /  1 % 6 * 40 + 55;
			return TerminalDriver::Color(r, g, b);
		}

		const uint8_t gray = (code - 232) * 10 + 8;
		return TerminalDriver::Color(gray, gray, gray);
	}

	BAN::Optional<TerminalDriver::Color> VirtualTTY::get_24bit_color()
	{
		ASSERT(m_ansi_state.nums[1] == 2);
		if (m_ansi_state.nums[2] < 0) return {};
		if (m_ansi_state.nums[3] < 0) return {};
		if (m_ansi_state.nums[4] < 0) return {};
		const uint8_t r = BAN::Math::min(m_ansi_state.nums[2], 255);
		const uint8_t g = BAN::Math::min(m_ansi_state.nums[3], 255);
		const uint8_t b = BAN::Math::min(m_ansi_state.nums[4], 255);
		return TerminalDriver::Color(r, g, b);
	}

	void VirtualTTY::handle_ansi_csi(uint8_t ch)
	{
		ASSERT(m_write_lock.current_processor_has_lock());
		switch (ch)
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				if (m_ansi_state.index >= m_ansi_state.max_nums)
					dwarnln("Only {} arguments supported with ANSI codes", m_ansi_state.max_nums);
				else
				{
					int32_t& val = m_ansi_state.nums[m_ansi_state.index];
					val = (val == -1) ? (ch - '0') : (val * 10 + ch - '0');
				}
				return;
			}
			case ';':
				m_ansi_state.index = BAN::Math::min<size_t>(m_ansi_state.index + 1, m_ansi_state.max_nums);
				return;
			case 'A': // Cursor Up
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_row = BAN::Math::max<int32_t>(m_row - m_ansi_state.nums[0], 0);
				return reset_ansi();
			case 'B': // Curson Down
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_row = BAN::Math::min<int32_t>(m_row + m_ansi_state.nums[0], m_height - 1);
				return reset_ansi();
			case 'C': // Cursor Forward
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_column = BAN::Math::min<int32_t>(m_column + m_ansi_state.nums[0], m_width - 1);
				return reset_ansi();
			case 'D': // Cursor Back
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_column = BAN::Math::max<int32_t>(m_column - m_ansi_state.nums[0], 0);
				return reset_ansi();
			case 'E': // Cursor Next Line
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_row = BAN::Math::min<int32_t>(m_row + m_ansi_state.nums[0], m_height - 1);
				m_column = 0;
				return reset_ansi();
			case 'F': // Cursor Previous Line
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_row = BAN::Math::max<int32_t>(m_row - m_ansi_state.nums[0], 0);
				m_column = 0;
				return reset_ansi();
			case 'G': // Cursor Horizontal Absolute
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_column = BAN::Math::clamp<int32_t>(m_ansi_state.nums[0] - 1, 0, m_width - 1);
				return reset_ansi();
			case 'H': // Cursor Position
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				if (m_ansi_state.nums[1] == -1)
					m_ansi_state.nums[1] = 1;
				m_row = BAN::Math::clamp<int32_t>(m_ansi_state.nums[0] - 1, 0, m_height - 1);
				m_column = BAN::Math::clamp<int32_t>(m_ansi_state.nums[1] - 1, 0, m_width - 1);
				return reset_ansi();
			case 'J': // Erase in Display
				if (m_ansi_state.nums[0] == -1 || m_ansi_state.nums[0] == 0)
				{
					// Clear from cursor to the end of screen
					for (uint32_t i = m_column; i < m_width; i++)
						putchar_at(' ', i, m_row);
					for (uint32_t row = 0; row < m_height; row++)
						for (uint32_t col = 0; col < m_width; col++)
							putchar_at(' ', col, row);
					return reset_ansi();
				}
				if (m_ansi_state.nums[0] == 1)
				{
					// Clear from cursor to the beginning of screen
					for (uint32_t row = 0; row < m_row; row++)
						for (uint32_t col = 0; col < m_width; col++)
							putchar_at(' ', col, row);
					for (uint32_t i = 0; i <= m_column; i++)
						putchar_at(' ', i, m_row);
					return reset_ansi();
				}
				if (m_ansi_state.nums[0] == 2 || m_ansi_state.nums[0] == 3)
				{
					// FIXME: if num == 3 clear scrollback buffer
					clear();
					return reset_ansi();
				}
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character J");
				return;
			case 'K': // Erase in Line
				if (m_ansi_state.nums[0] == -1 || m_ansi_state.nums[0] == 0)
				{
					for (uint32_t i = m_column; i < m_width; i++)
						putchar_at(' ', i, m_row);
					return reset_ansi();
				}
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character K");
				return;
			case 'L': // Insert Line
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				for (uint32_t row = m_height; row > m_row; row--)
				{
					const uint32_t dst_y = row - 1;
					if (const uint32_t src_y = dst_y - m_ansi_state.nums[0]; src_y < dst_y)
						memcpy(&m_buffer[dst_y * m_width], &m_buffer[src_y * m_width], m_width * sizeof(Cell));
					for (uint32_t x = 0; x < m_width; x++)
						render_from_buffer(x, dst_y);
				}
				for (uint32_t y_off = 0; y_off < (uint32_t)m_ansi_state.nums[0] && m_row + y_off < m_height; y_off++)
					for (uint32_t x = 0; x < m_width; x++)
						putchar_at(' ', x, m_row + y_off);
				return reset_ansi();
			case 'M':
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				if (m_row + m_ansi_state.nums[0] >= m_height)
					m_ansi_state.nums[0] = m_height - m_row - 1;
				for (uint32_t row = m_row; row < m_height; row++)
				{
					const uint32_t dst_y = row;
					const uint32_t src_y = dst_y + m_ansi_state.nums[0];
					memcpy(&m_buffer[dst_y * m_width], &m_buffer[src_y * m_width], m_width * sizeof(Cell));
					for (uint32_t x = 0; x < m_width; x++)
						render_from_buffer(x, dst_y);
				}
				for (uint32_t y_off = 0; y_off < (uint32_t)m_ansi_state.nums[0]; y_off++)
					for (uint32_t x = 0; x < m_width; x++)
						putchar_at(' ', x, m_height - y_off - 1);
				return reset_ansi();
			case 'S': // Scroll Up
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character S");
				return;
			case 'T': // Scroll Down
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character T");
				return;
			case 'f': // Horizontal Vertical Position
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character f");
				return;
			case 'm':
				if (m_ansi_state.nums[0] == 38 || m_ansi_state.nums[0] == 48)
				{
					if (m_ansi_state.nums[1] != 5 && m_ansi_state.nums[1] != 2)
					{
						reset_ansi();
						dprintln_if(DEBUG_VTTY, "Unsupported ANSI SGR {}", m_ansi_state.nums[1]);
						return;
					}
					const auto color = (m_ansi_state.nums[1] == 5)
						? get_8bit_color()
						: get_24bit_color();
					if (color.has_value())
						(m_ansi_state.nums[0] == 38 ? m_foreground : m_background) = *color;
					return reset_ansi();
				}
				for (size_t i = 0; i <= m_ansi_state.index && i < m_ansi_state.max_nums; i++)
					handle_ansi_csi_color(BAN::Math::max(m_ansi_state.nums[i], 0));
				return reset_ansi();
			case 's':
				m_saved_row = m_row;
				m_saved_column = m_column;
				return reset_ansi();
			case 'u':
				m_row = m_saved_row;
				m_column = m_saved_column;
				return reset_ansi();
			case '@':
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_ansi_state.nums[0] = BAN::Math::min<uint32_t>(m_ansi_state.nums[0], m_width - m_column);
				memmove(
					&m_buffer[m_row * m_width + m_column],
					&m_buffer[m_row * m_width + m_column + m_ansi_state.nums[0]],
					m_width - m_column - m_ansi_state.nums[0]
				);
				for (int i = 0; i < m_ansi_state.nums[0]; i++)
					putchar_at(' ', m_column + i, m_row);
				for (uint32_t x = m_column + m_ansi_state.nums[0]; x < m_width; x++)
					render_from_buffer(x, m_row);
				return reset_ansi();
			case 'b':
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				if (m_last_graphic_char)
					for (int i = 0; i < m_ansi_state.nums[0]; i++)
						putcodepoint(m_last_graphic_char);
				return reset_ansi();
			case 'd':
				if (m_ansi_state.nums[0] == -1)
					m_ansi_state.nums[0] = 1;
				m_row = BAN::Math::clamp<uint32_t>(m_ansi_state.nums[0], 1, m_height) - 1;
				return reset_ansi();
			case '?':
				if (m_ansi_state.index == 0 || m_ansi_state.nums[0] == -1)
				{
					m_ansi_state.question = true;
					return;
				}
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "invalid ANSI CSI ?");
				return;
			case 'h':
			case 'l':
				if (m_ansi_state.question && m_ansi_state.nums[0] == 25)
				{
					m_cursor_shown = (ch == 'h');
					return reset_ansi();
				}
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "invalid ANSI CSI character {}", static_cast<char>(ch));
				return;
			case 'n':
				if (m_ansi_state.nums[0] == 6)
				{
					char buffer[2 + 10 + 1 + 10 + 1];
					size_t len = 0;
					BAN::Formatter::print([&](char ch) { buffer[len++] = ch; }, "\e[{};{}R", m_row + 1, m_column + 1);
					for (size_t i = 0; i < len; i++)
						handle_input_byte(buffer[i]);
					return reset_ansi();
				};
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character n");
				return;
			default:
				reset_ansi();
				dprintln_if(DEBUG_VTTY, "Unsupported ANSI CSI character {}", static_cast<char>(ch));
				return;
		}
	}

	void VirtualTTY::render_from_buffer(uint32_t x, uint32_t y)
	{
		ASSERT(m_write_lock.current_processor_has_lock());
		ASSERT(x < m_width && y < m_height);
		const auto& cell = m_buffer[y * m_width + x];
		m_terminal_driver->putchar_at(cell.codepoint, x, y, cell.foreground, cell.background);
	}

	void VirtualTTY::putchar_at(uint32_t codepoint, uint32_t x, uint32_t y)
	{
		ASSERT(m_write_lock.current_processor_has_lock());
		ASSERT(x < m_width && y < m_height);
		auto& cell = m_buffer[y * m_width + x];
		cell.codepoint = codepoint;
		cell.foreground = m_colors_inverted ? m_background : m_foreground;
		cell.background = m_colors_inverted ? m_foreground : m_background;
		m_terminal_driver->putchar_at(codepoint, x, y, cell.foreground, cell.background);
	}

	void VirtualTTY::scroll_if_needed()
	{
		while (m_row >= m_height)
		{
			memmove(m_buffer, m_buffer + m_width, m_width * (m_height - 1) * sizeof(Cell));

			// Clear last line in buffer
			for (uint32_t x = 0; x < m_width; x++)
				m_buffer[(m_height - 1) * m_width + x] = { .foreground = m_foreground, .background = m_background, .codepoint = ' ' };

			if (!m_terminal_driver->scroll(m_background))
			{
				// No fast scrolling, render the whole buffer to the screen
				for (uint32_t y = 0; y < m_height; y++)
					for (uint32_t x = 0; x < m_width; x++)
						render_from_buffer(x, y);
			}

			m_column = 0;
			m_row--;
		}
	}

	void VirtualTTY::putcodepoint(uint32_t codepoint)
	{
		ASSERT(m_write_lock.current_processor_has_lock());

		switch (codepoint)
		{
			case BEL: // TODO
				break;
			case BS:
				if (m_column > 0)
					m_column--;
				break;
			case HT:
				m_column++;
				while (m_column % 8)
					m_column++;
				break;
			case LF:
				m_column = 0;
				m_row++;
				break;
			case FF:
				m_row++;
				break;
			case CR:
				m_column = 0;
				break;
			case ESC:
				m_state = State::WaitingAnsiEscape;
				break;
			default:
				if (m_column >= m_width)
				{
					m_column = 0;
					m_row++;
				}
				scroll_if_needed();
				putchar_at(codepoint, m_column, m_row);
				m_last_graphic_char = codepoint;
				m_column++;
				break;
		}

		scroll_if_needed();
	}

	bool VirtualTTY::putchar_impl(uint8_t ch)
	{
		ASSERT(m_write_lock.current_processor_has_lock());

		uint32_t codepoint = ch;

		switch (m_state)
		{
			case State::Normal:
				if ((ch & 0x80) == 0)
					break;
				if ((ch & 0xE0) == 0xC0)
				{
					m_utf8_state.codepoint = ch & 0x1F;
					m_utf8_state.bytes_missing = 1;
				}
				else if ((ch & 0xF0) == 0xE0)
				{
					m_utf8_state.codepoint = ch & 0x0F;
					m_utf8_state.bytes_missing = 2;
				}
				else if ((ch & 0xF8) == 0xF0)
				{
					m_utf8_state.codepoint = ch & 0x07;
					m_utf8_state.bytes_missing = 3;
				}
				else
				{
					reset_ansi();
					dprintln_if(DEBUG_VTTY, "invalid utf8");
					return true;
				}
				m_state = State::WaitingUTF8;
				return true;
			case State::WaitingAnsiEscape:
				if (ch == CSI)
					m_state = State::WaitingAnsiCSI;
				else
				{
					reset_ansi();
					dprintln_if(DEBUG_VTTY, "unsupported byte after ansi escape {2H}", (uint8_t)ch);
				}
				return true;
			case State::WaitingAnsiCSI:
				handle_ansi_csi(ch);
				return true;
			case State::WaitingUTF8:
				if ((ch & 0xC0) != 0x80)
				{
					m_state = State::Normal;
					dprintln_if(DEBUG_VTTY, "invalid utf8");
					return true;
				}
				m_utf8_state.codepoint = (m_utf8_state.codepoint << 6) | (ch & 0x3F);
				m_utf8_state.bytes_missing--;
				if (m_utf8_state.bytes_missing)
					return true;
				m_state = State::Normal;
				codepoint = m_utf8_state.codepoint;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		putcodepoint(codepoint);
		return true;
	}

	void VirtualTTY::update_cursor()
	{
		if (m_cursor_shown != m_last_cursor_shown)
			m_terminal_driver->set_cursor_shown(m_cursor_shown);
		m_last_cursor_shown = m_cursor_shown;

		if (m_last_cursor_column != m_column || m_last_cursor_row != m_row)
			m_terminal_driver->set_cursor_position(m_column, m_row);
		m_last_cursor_column = m_column;
		m_last_cursor_row = m_row;
	}

}
