#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/Serial.h>
#include <kernel/TTY.h>
#include <kernel/VESA.h>

#include <string.h>

#define BEL	0x07
#define BS	0x08
#define HT	0x09
#define LF	0x0A
#define FF	0x0C
#define CR	0x0D
#define ESC	0x1B

#define CSI '['

template<typename T> inline constexpr T max(T a, T b)			{ return a > b ? a : b; }
template<typename T> inline constexpr T min(T a, T b)			{ return a < b ? a : b; }
template<typename T> inline constexpr T clamp(T x, T a, T b)	{ return x < a ? a : x > b ? b : x; }

static TTY* s_tty = nullptr;

TTY::TTY()
{
	m_width = VESA::GetTerminalWidth();
	m_height = VESA::GetTerminalHeight();
	
	m_buffer = new Cell[m_width * m_height];

	if (s_tty == nullptr)
		s_tty = this;
}

void TTY::Clear()
{
	for (size_t i = 0; i < m_width * m_height; i++)
		m_buffer[i] = { .foreground = m_foreground, .background = m_background, .character = ' ' };
	VESA::Clear(m_background);
}

void TTY::SetCursorPosition(uint32_t x, uint32_t y)
{
	static uint32_t last_x = -1;
	static uint32_t last_y = -1;
	if (last_x != uint32_t(-1) && last_y != uint32_t(-1))
		RenderFromBuffer(last_x, last_y); // Hacky way to clear previous cursor in graphics mode :D
	VESA::SetCursorPosition(x, y, VESA::Color::BRIGHT_WHITE);
	last_x = m_column = x;
	last_y = m_row = y;
}

static uint16_t handle_unicode(uint8_t ch)
{
	static uint8_t	unicode_left = 0;
	static uint16_t	codepoint = 0;

	if (unicode_left)
	{
		if ((ch >> 6) == 0b10)
		{
			codepoint = (codepoint << 6) | ch;
			unicode_left--;
			if (unicode_left > 0)
				return 0xFFFF;
			return codepoint;
		}
		else
		{
			// invalid utf-8
			unicode_left = 0;
			return 0x00;
		}
	}
	else
	{
		if ((ch >> 3) == 0b11110)
		{
			unicode_left = 3;
			codepoint = ch & 0b00000111;
			return 0xFFFF;
		}
		if ((ch >> 4) == 0b1110)
		{
			unicode_left = 2;
			codepoint = ch & 0b00001111;
			return 0xFFFF;
		}
		if ((ch >> 5) == 0b110)
		{
			unicode_left = 1;
			codepoint = ch & 0b00011111;
			return 0xFFFF;
		}
	}

	return ch & 0x7F;
}

void TTY::ResetAnsiEscape()
{
	m_ansi_state.mode = '\0';
	m_ansi_state.index = 0;
	m_ansi_state.nums[0] = -1;
	m_ansi_state.nums[1] = -1;
}

void TTY::HandleAnsiSGR()
{
	switch (m_ansi_state.nums[0])
	{
		case -1:
		case 0:
			m_foreground = VESA::Color::BRIGHT_WHITE;
			m_background = VESA::Color::BLACK;
			break;

		case 30: m_foreground = VESA::Color::BRIGHT_BLACK;		break;
		case 31: m_foreground = VESA::Color::BRIGHT_RED;		break;
		case 32: m_foreground = VESA::Color::BRIGHT_GREEN;		break;
		case 33: m_foreground = VESA::Color::BRIGHT_YELLOW;		break;
		case 34: m_foreground = VESA::Color::BRIGHT_BLUE;		break;
		case 35: m_foreground = VESA::Color::BRIGHT_MAGENTA;	break;
		case 36: m_foreground = VESA::Color::BRIGHT_CYAN;		break;
		case 37: m_foreground = VESA::Color::BRIGHT_WHITE;		break;

		case 40: m_background = VESA::Color::BRIGHT_BLACK;		break;
		case 41: m_background = VESA::Color::BRIGHT_RED;		break;
		case 42: m_background = VESA::Color::BRIGHT_GREEN;		break;
		case 43: m_background = VESA::Color::BRIGHT_YELLOW;		break;
		case 44: m_background = VESA::Color::BRIGHT_BLUE;		break;
		case 45: m_background = VESA::Color::BRIGHT_MAGENTA;	break;
		case 46: m_background = VESA::Color::BRIGHT_CYAN;		break;
		case 47: m_background = VESA::Color::BRIGHT_WHITE;		break;
	}
}

void TTY::HandleAnsiEscape(uint16_t ch)
{
	switch (m_ansi_state.mode)
	{
		case '\1':
		{
			if (ch == CSI)
			{
				m_ansi_state.mode = CSI;
				return;
			}
			return ResetAnsiEscape();
		}

		case CSI:
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
					m_row = max<int32_t>(m_row - m_ansi_state.nums[0], 0);
					return ResetAnsiEscape();
				case 'B': // Curson Down
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_row = min<int32_t>(m_row + m_ansi_state.nums[0], m_height - 1);
					return ResetAnsiEscape();
				case 'C': // Cursor Forward
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_column = min<int32_t>(m_column + m_ansi_state.nums[0], m_width - 1);
					return ResetAnsiEscape();
				case 'D': // Cursor Back
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_column = max<int32_t>(m_column - m_ansi_state.nums[0], 0);
					return ResetAnsiEscape();
				case 'E': // Cursor Next Line
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_row = min<int32_t>(m_row + m_ansi_state.nums[0], m_height - 1);
					m_column = 0;
					return ResetAnsiEscape();
				case 'F': // Cursor Previous Line
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_row = max<int32_t>(m_row - m_ansi_state.nums[0], 0);
					m_column = 0;
					return ResetAnsiEscape();
				case 'G': // Cursor Horizontal Absolute
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					m_column = clamp<int32_t>(m_ansi_state.nums[0] - 1, 0, m_width - 1);
					return ResetAnsiEscape();
				case 'H': // Cursor Position
					if (m_ansi_state.nums[0] == -1)
						m_ansi_state.nums[0] = 1;
					if (m_ansi_state.nums[1] == -1)
						m_ansi_state.nums[1] = 1;
					m_row = clamp<int32_t>(m_ansi_state.nums[0] - 1, 0, m_height - 1);
					m_column = clamp<int32_t>(m_ansi_state.nums[1] - 1, 0, m_width - 1);
					return ResetAnsiEscape();
				case 'J': // Erase in Display
					dprintln("Unsupported ANSI CSI character J");
					return ResetAnsiEscape();
				case 'K': // Erase in Line
					dprintln("Unsupported ANSI CSI character K");
					return ResetAnsiEscape();
				case 'S': // Scroll Up
					dprintln("Unsupported ANSI CSI character S");
					return ResetAnsiEscape();
				case 'T': // Scroll Down
					dprintln("Unsupported ANSI CSI character T");
					return ResetAnsiEscape();
				case 'f': // Horizontal Vertical Position
					dprintln("Unsupported ANSI CSI character f");
					return ResetAnsiEscape();
				case 'm':
					HandleAnsiSGR();
					return ResetAnsiEscape();
				default:
					dprintln("Unsupported ANSI CSI character {}", ch);
					return ResetAnsiEscape();
			}
		}

		default:
			dprintln("Unsupported ANSI mode");
			return ResetAnsiEscape();
	}
}

void TTY::PutCharAt(uint16_t ch, size_t x, size_t y)
{
	auto& cell = m_buffer[y * m_width + x];
	cell.character = ch;
	cell.foreground = m_foreground;
	cell.background = m_background;
	VESA::PutCharAt(ch, x, y, m_foreground, m_background);
}

void TTY::PutChar(char ch)
{
	uint16_t cp = handle_unicode(ch);
	if (cp == 0xFFFF)
		return;

	if (m_ansi_state.mode != 0)
		return HandleAnsiEscape(cp);

	// https://en.wikipedia.org/wiki/ANSI_escape_code
	switch (cp)
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
			m_ansi_state.mode = '\1';
			break;
		default:
			PutCharAt(cp, m_column, m_row);
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
		for (size_t x = 0; x < m_width; x++)
			m_buffer[(m_height - 1) * m_width + x] = { .foreground = m_foreground, .background = m_background, .character = ' ' };

		// Render the whole buffer to the screen
		for (size_t y = 0; y < m_height; y++)
			for (size_t x = 0; x < m_width; x++)
				RenderFromBuffer(x, y);

		m_column = 0;
		m_row--;
	}

	SetCursorPosition(m_column, m_row);
}

void TTY::Write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		PutChar(data[i]);
}

void TTY::WriteString(const char* data)
{
	while (*data)
	{
		PutChar(*data);
		data++;
	}
}

void TTY::PutCharCurrent(char ch)
{
	if (s_tty)
	{
		s_tty->PutChar(ch);
	}
	else
	{
		static size_t x = 0;
		static size_t y = 0;

		switch (ch)
		{
		case '\n':
			x = 0;
			y++;
			break;
		default:
			VESA::PutCharAt(ch, x, y, VESA::Color::BRIGHT_WHITE, VESA::Color::BLACK);
			x++;
			break;
		}

		if (x == VESA::GetTerminalWidth())
		{
			x = 0;
			y++;
		}

		if (y == VESA::GetTerminalHeight())
		{
			x = 0;
			y = 0;
			VESA::Clear(VESA::Color::BLACK);
		}
	}
}
