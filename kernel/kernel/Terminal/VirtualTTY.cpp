#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Terminal/VirtualTTY.h>

#include <fcntl.h>
#include <string.h>
#include <sys/sysmacros.h>

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

	static dev_t next_rdev()
	{
		static dev_t major = DevFileSystem::get().get_next_dev();
		static dev_t minor = 0;
		return makedev(major, minor++);
	}

	BAN::ErrorOr<BAN::RefPtr<VirtualTTY>> VirtualTTY::create(TerminalDriver* driver)
	{
		auto* tty = new VirtualTTY(driver);
		ASSERT(tty);

		auto ref_ptr = BAN::RefPtr<VirtualTTY>::adopt(tty);
		DevFileSystem::get().add_device(ref_ptr->name(), ref_ptr);
		return ref_ptr;
	}

	VirtualTTY::VirtualTTY(TerminalDriver* driver)
		: TTY(0666, 0, 0)
		, m_terminal_driver(driver)
		, m_rdev(next_rdev())
	{
		m_name = BAN::String::formatted("tty{}", minor(rdev()));

		m_width = m_terminal_driver->width();
		m_height = m_terminal_driver->height();

		m_buffer = new Cell[m_width * m_height];
		ASSERT(m_buffer);
	}

	void VirtualTTY::clear()
	{
		for (uint32_t i = 0; i < m_width * m_height; i++)
			m_buffer[i] = { .foreground = m_foreground, .background = m_background, .codepoint = ' ' };
		m_terminal_driver->clear(m_background);
	}

	void VirtualTTY::set_font(const Kernel::Font& font)
	{
		m_terminal_driver->set_font(font);

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
	}

	void VirtualTTY::set_cursor_position(uint32_t x, uint32_t y)
	{
		static uint32_t last_x = -1;
		static uint32_t last_y = -1;
		if (last_x != uint32_t(-1) && last_y != uint32_t(-1))
			render_from_buffer(last_x, last_y);
		if (m_show_cursor)
			m_terminal_driver->set_cursor_position(x, y);
		last_x = m_column = x;
		last_y = m_row = y;
	}

	void VirtualTTY::reset_ansi()
	{
		m_ansi_state.index = 0;
		m_ansi_state.nums[0] = -1;
		m_ansi_state.nums[1] = -1;
		m_ansi_state.question = false;
		m_state = State::Normal;
	}

	void VirtualTTY::handle_ansi_csi_color()
	{
		switch (m_ansi_state.nums[0])
		{
			case -1:
			case 0:
				m_foreground = TerminalColor::BRIGHT_WHITE;
				m_background = TerminalColor::BLACK;
				break;

			case 30: m_foreground = TerminalColor::BRIGHT_BLACK;	break;
			case 31: m_foreground = TerminalColor::BRIGHT_RED;		break;
			case 32: m_foreground = TerminalColor::BRIGHT_GREEN;	break;
			case 33: m_foreground = TerminalColor::BRIGHT_YELLOW;	break;
			case 34: m_foreground = TerminalColor::BRIGHT_BLUE;		break;
			case 35: m_foreground = TerminalColor::BRIGHT_MAGENTA;	break;
			case 36: m_foreground = TerminalColor::BRIGHT_CYAN;		break;
			case 37: m_foreground = TerminalColor::BRIGHT_WHITE;	break;

			case 40: m_background = TerminalColor::BRIGHT_BLACK;	break;
			case 41: m_background = TerminalColor::BRIGHT_RED;		break;
			case 42: m_background = TerminalColor::BRIGHT_GREEN;	break;
			case 43: m_background = TerminalColor::BRIGHT_YELLOW;	break;
			case 44: m_background = TerminalColor::BRIGHT_BLUE;		break;
			case 45: m_background = TerminalColor::BRIGHT_MAGENTA;	break;
			case 46: m_background = TerminalColor::BRIGHT_CYAN;		break;
			case 47: m_background = TerminalColor::BRIGHT_WHITE;	break;
		}
	}

	void VirtualTTY::handle_ansi_csi(uint8_t ch)
	{
		switch (ch)
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				int32_t& val = m_ansi_state.nums[m_ansi_state.index];
				val = (val == -1) ? (ch - '0') : (val * 10 + ch - '0');
				return;
			}
			case ';':
				m_ansi_state.index++;
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
				}
				else if (m_ansi_state.nums[0] == 1)
				{
					// Clear from cursor to the beginning of screen
					for (uint32_t row = 0; row < m_row; row++)
						for (uint32_t col = 0; col < m_width; col++)
							putchar_at(' ', col, row);
					for (uint32_t i = 0; i <= m_column; i++)
						putchar_at(' ', i, m_row);
				}
				else if (m_ansi_state.nums[0] == 2 || m_ansi_state.nums[0] == 3)
				{
					// Clean entire screen
					clear();
				}
				else
				{
					dprintln("Unsupported ANSI CSI character J");
				}
				
				if (m_ansi_state.nums[0] == 3)
				{
					// FIXME: Clear scroll backbuffer if/when added
				}
				return reset_ansi();
			case 'K': // Erase in Line
				if (m_ansi_state.nums[0] == -1 || m_ansi_state.nums[0] == 0)
					for (uint32_t i = m_column; i < m_width; i++)
						putchar_at(' ', i, m_row);
				else
					dprintln("Unsupported ANSI CSI character K");
				return reset_ansi();
			case 'S': // Scroll Up
				dprintln("Unsupported ANSI CSI character S");
				return reset_ansi();
			case 'T': // Scroll Down
				dprintln("Unsupported ANSI CSI character T");
				return reset_ansi();
			case 'f': // Horizontal Vertical Position
				dprintln("Unsupported ANSI CSI character f");
				return reset_ansi();
			case 'm':
				handle_ansi_csi_color();
				return reset_ansi();
			case 's':
				m_saved_row = m_row;
				m_saved_column = m_column;
				return reset_ansi();
			case 'u':
				m_row = m_saved_row;
				m_column = m_saved_column;
				return reset_ansi();

			case '?':
				if (m_ansi_state.index != 0 || m_ansi_state.nums[0] != -1)
				{
					dprintln("invalid ANSI CSI ?");
					return reset_ansi();
				}
				m_ansi_state.question = true;
				return;
			case 'h':
			case 'l':
				if (!m_ansi_state.question || m_ansi_state.nums[0] != 25)
				{
					dprintln("invalid ANSI CSI ?{}{}", m_ansi_state.nums[0], (char)ch);
					return reset_ansi();
				}
				m_show_cursor = (ch == 'h');
				return reset_ansi();
			default:
				dprintln("Unsupported ANSI CSI character {}", ch);
				return reset_ansi();
		}
	}

	void VirtualTTY::render_from_buffer(uint32_t x, uint32_t y)
	{
		ASSERT(x < m_width && y < m_height);
		const auto& cell = m_buffer[y * m_width + x];
		m_terminal_driver->putchar_at(cell.codepoint, x, y, cell.foreground, cell.background);
	}

	void VirtualTTY::putchar_at(uint32_t codepoint, uint32_t x, uint32_t y)
	{
		ASSERT(x < m_width && y < m_height);
		auto& cell = m_buffer[y * m_width + x];
		cell.codepoint = codepoint;
		cell.foreground = m_foreground;
		cell.background = m_background;
		m_terminal_driver->putchar_at(codepoint, x, y, m_foreground, m_background);
	}

	void VirtualTTY::putchar_impl(uint8_t ch)
	{
		ASSERT(m_lock.is_locked());
		
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
					dprintln("invalid utf8");
				}
				m_state = State::WaitingUTF8;
				return;
			case State::WaitingAnsiEscape:
				if (ch == CSI)
					m_state = State::WaitingAnsiCSI;
				else
				{
					dprintln("unsupported byte after ansi escape {2H}", (uint8_t)ch);
					reset_ansi();
				}
				return;
			case State::WaitingAnsiCSI:
				handle_ansi_csi(ch);
				set_cursor_position(m_column, m_row);
				return;
			case State::WaitingUTF8:
				if ((ch & 0xC0) != 0x80)
				{
					dprintln("invalid utf8");
					m_state = State::Normal;
					return;
				}
				m_utf8_state.codepoint = (m_utf8_state.codepoint << 6) | (ch & 0x3F);
				m_utf8_state.bytes_missing--;
				if (m_utf8_state.bytes_missing)
					return;
				m_state = State::Normal;
				codepoint = m_utf8_state.codepoint;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

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
				break;;
			default:
				putchar_at(codepoint, m_column, m_row);
				m_column++;
				break;
		}

		if (m_column >= m_width)
		{
			m_column = 0;
			m_row++;
		}

		while (m_row >= m_height)
		{
			memmove(m_buffer, m_buffer + m_width, m_width * (m_height - 1) * sizeof(Cell));

			// Clear last line in buffer
			for (uint32_t x = 0; x < m_width; x++)
				m_buffer[(m_height - 1) * m_width + x] = { .foreground = m_foreground, .background = m_background, .codepoint = ' ' };

			// Render the whole buffer to the screen
			for (uint32_t y = 0; y < m_height; y++)
				for (uint32_t x = 0; x < m_width; x++)
					render_from_buffer(x, y);

			m_column = 0;
			m_row--;
		}

		set_cursor_position(m_column, m_row);
	}

}