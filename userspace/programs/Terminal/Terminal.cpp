#include "Terminal.h"

#include <BAN/Debug.h>
#include <BAN/UTF8.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

static constexpr uint32_t s_colors_dark[] {
	0xCC'000000,
	0xCC'FF0000,
	0xCC'00FF00,
	0xCC'FFFF00,
	0xCC'0000FF,
	0xCC'FF00FF,
	0xCC'00FFFF,
	0xCC'BFBFBF,
};

static constexpr uint32_t s_colors_bright[] {
	0xCC'3F3F3F,
	0xCC'FF7F7F,
	0xCC'7FFF7F,
	0xCC'FFFF7F,
	0xCC'7F7FFF,
	0xCC'FF7FFF,
	0xCC'7FFFFF,
	0xCC'FFFFFF,
};

static constexpr auto s_default_bg_color = s_colors_dark[0];
static constexpr auto s_default_fg_color = s_colors_bright[7];

void Terminal::start_shell()
{
	int pts_master = posix_openpt(O_RDWR | O_NOCTTY);
	if (pts_master == -1)
	{
		dwarnln("posix_openpt: {}", strerror(errno));
		exit(1);
	}

	if (grantpt(pts_master) == -1)
	{
		dwarnln("grantpt: {}", strerror(errno));
		exit(1);
	}

	if (unlockpt(pts_master) == -1)
	{
		dwarnln("unlockpt: {}", strerror(errno));
		exit(1);
	}

	pid_t shell_pid = fork();
	if (shell_pid == 0)
	{
		if (setsid() == -1)
		{
			dwarnln("setsid: {}", strerror(errno));
			exit(1);
		}

		char* pts_slave_name = ptsname(pts_master);
		if (pts_slave_name == nullptr)
		{
			dwarnln("ptsname: {}", strerror(errno));
			exit(1);
		}

		int pts_slave = open(pts_slave_name, O_RDWR);
		if (pts_slave == -1)
		{
			dwarnln("open: {}", strerror(errno));
			exit(1);
		}

		if (dup2(pts_slave, STDIN_FILENO) == -1 || dup2(pts_slave, STDOUT_FILENO) == -1 || dup2(pts_slave, STDERR_FILENO) == -1)
		{
			dwarnln("dup2: {}", strerror(errno));
			exit(1);
		}

		close(pts_slave);
		close(pts_master);

		execl("/bin/Shell", "Shell", NULL);
		exit(1);
	}

	if (shell_pid == -1)
	{
		dwarnln("fork: {}", strerror(errno));
		exit(1);
	}

	m_shell_info = {
		.pts_master = pts_master,
		.pid = shell_pid
	};
}

static volatile bool s_shell_exited = false;

void Terminal::run()
{
	signal(SIGCHLD, [](int) { s_shell_exited = true; });
	start_shell();

	m_bg_color = s_default_bg_color;
	m_fg_color = s_default_fg_color;

	auto attributes = LibGUI::Window::default_attributes;
	attributes.alpha_channel = true;
	attributes.resizable = true;

	m_window = MUST(LibGUI::Window::create(600, 400, "Terminal"_sv, attributes));
	m_window->fill(m_bg_color);
	m_window->invalidate();
	m_window->set_bg_color(m_bg_color);

	m_font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv));

	m_window->set_min_size(m_font.width() * 8, m_font.height() * 2);

	{
		winsize winsize;
		winsize.ws_col = cols();
		winsize.ws_row = rows();
		if (ioctl(m_shell_info.pts_master, TIOCSWINSZ, &winsize) == -1)
			perror("ioctl");
	}

	{
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		m_cursor_blink_shown = true;
		m_cursor_blink_ms = ts.tv_sec * 1'000 + ts.tv_nsec / 1'000'000;
	}

	MUST(m_cursor_buffer.resize(m_font.width() * m_font.height(), m_bg_color));
	show_cursor();

	m_window->set_key_event_callback([&](LibGUI::EventPacket::KeyEvent::event_t event) { on_key_event(event); });
	m_window->set_resize_window_event_callback([&] {
		if (const auto rem = m_window->height() % m_font.height())
		{
			m_window->fill_rect(0, m_window->height() - rem, m_window->width(), rem, m_bg_color);
			m_window->invalidate(0, m_window->height() - rem, m_window->width(), rem);
		}

		if (const auto rem = m_window->width() % m_font.width())
		{
			m_window->fill_rect(m_window->width() - rem, 0, rem, m_window->height(), m_bg_color);
			m_window->invalidate(m_window->width() - rem, 0, rem, m_window->height());
		}

		if (m_cursor.x < cols() && m_cursor.y < rows())
			return;

		m_cursor.x = BAN::Math::min(m_cursor.x, cols() - 1);
		m_cursor.y = BAN::Math::min(m_cursor.y, rows() - 1);
		for (auto& pixel : m_cursor_buffer)
			pixel = m_bg_color;
	});

	const int max_fd = BAN::Math::max(m_shell_info.pts_master, m_window->server_fd());
	while (!s_shell_exited)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(m_shell_info.pts_master, &fds);
		FD_SET(m_window->server_fd(), &fds);

		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		const uint64_t current_ms = ts.tv_sec * 1'000 + ts.tv_nsec / 1'000'000;

		const uint64_t ms_until_blink = 500 - BAN::Math::min<uint64_t>(current_ms - m_cursor_blink_ms, 500);

		timeval timeout;
		timeout.tv_sec = ms_until_blink / 1'000;
		timeout.tv_usec = ms_until_blink * 1'000;
		if (select(max_fd + 1, &fds, nullptr, nullptr, &timeout) == 0)
		{
			m_cursor_blink_shown = !m_cursor_blink_shown;
			m_cursor_blink_ms = current_ms + ms_until_blink;
		}

		m_got_key_event = false;

		hide_cursor();
		if (FD_ISSET(m_shell_info.pts_master, &fds))
			if (!read_shell())
				break;
		if (FD_ISSET(m_window->server_fd(), &fds))
			m_window->poll_events();
		if (m_got_key_event)
		{
			m_cursor_blink_shown = true;
			m_cursor_blink_ms = current_ms;
		}
		show_cursor();
	}
}

void Terminal::hide_cursor()
{
	if (m_cursor.x == cols())
		return;
	const uint32_t cursor_base_x = m_cursor.x * m_font.width();
	const uint32_t cursor_base_y = m_cursor.y * m_font.height();
	for (uint32_t y = 0; y < m_font.height(); y++)
		for (uint32_t x = 0; x < m_font.width(); x++)
			m_window->set_pixel(cursor_base_x + x, cursor_base_y + y, m_cursor_buffer[y * m_font.width() + x]);
	m_window->invalidate(cursor_base_x, cursor_base_y, m_font.width(), m_font.height());
}

void Terminal::show_cursor()
{
	if (m_cursor.x == cols())
		return;
	const uint32_t cursor_base_x = m_cursor.x * m_font.width();
	const uint32_t cursor_base_y = m_cursor.y * m_font.height();
	for (uint32_t y = 0; y < m_font.height(); y++)
		for (uint32_t x = 0; x < m_font.width(); x++)
			m_cursor_buffer[y * m_font.width() + x] = m_window->get_pixel(cursor_base_x + x, cursor_base_y + y);
	if (m_cursor_shown && m_cursor_blink_shown)
	{
		for (uint32_t y = m_font.height() * 13 / 16; y < m_font.height() - 1; y++)
			for (uint32_t x = 0; x < m_font.width(); x++)
				m_window->set_pixel(cursor_base_x + x, cursor_base_y + y, 0xFFFFFFFF);
		m_window->invalidate(cursor_base_x, cursor_base_y, m_font.width(), m_font.height());
	}
}

bool Terminal::read_shell()
{
	char buffer[512];
	ssize_t nread = read(m_shell_info.pts_master, buffer, sizeof(buffer));
	if (nread < 0)
		dwarnln("read: {}", strerror(errno));
	if (nread <= 0)
		return false;

	Rectangle should_invalidate;

	ssize_t i = 0;
	while (i < nread)
	{
		// all ansi escape codes must be handled
		if (buffer[i] == '\e')
		{
			while (i < nread)
			{
				char ch = buffer[i++];
				should_invalidate = should_invalidate.get_bounding_box(putchar(ch));
				if (isalpha(ch))
					break;
			}
			continue;
		}

		// find the next ansi escape code or end of buffer
		ssize_t non_ansi_end = i;
		while (non_ansi_end < nread && buffer[non_ansi_end] != '\e')
			non_ansi_end++;

		// we only need to process maximum of `rows()` newlines.
		// anything before that would get overwritten anyway
		ssize_t start = non_ansi_end;
		size_t newline_count = 0;
		while (start > i && newline_count < rows())
			newline_count += (buffer[--start] == '\n');

		// do possible scrolling already in here, so `putchar()` doesnt
		// have to scroll up to `rows()` times
		if (m_cursor.y + newline_count >= rows())
		{
			const uint32_t scroll = m_cursor.y + newline_count - rows() + 1;
			m_cursor.y -= scroll;
			m_window->shift_vertical(-scroll * (int32_t)m_font.height(), m_bg_color);
			should_invalidate = { 0, 0, m_window->width(), m_window->height() };
		}

		i = start;
		for (i = start; i < non_ansi_end; i++)
			should_invalidate = should_invalidate.get_bounding_box(putchar(buffer[i]));
	}

	if (should_invalidate.height && should_invalidate.width)
	{
		m_window->invalidate(
			should_invalidate.x,
			should_invalidate.y,
			should_invalidate.width,
			should_invalidate.height
		);
	}

	return true;
}

void Terminal::handle_sgr(int32_t value)
{
	switch (value)
	{
		case -1: case 0:
			m_bg_color = s_default_bg_color;
			m_fg_color = s_default_fg_color;
			m_colors_inverted = false;
			break;
		case 1:
			// FIXME: bold
			break;
		case 7:
			m_colors_inverted = true;
			break;
		case 10:
			// default font
			break;
		case 27:
			m_colors_inverted = false;
			break;
		case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
			m_fg_color = s_colors_dark[value - 30];
			break;
		case 39:
			m_fg_color = s_default_fg_color;
			break;
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
			m_bg_color = s_colors_dark[value - 40];
			break;
		case 49:
			m_bg_color = s_default_bg_color;
			break;
		case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
			m_fg_color = s_colors_bright[value - 90];
			break;
		case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
			m_bg_color = s_colors_bright[value - 100];
			break;
		default:
			dprintln("TODO: SGR {}", value);
			break;
	}
}

BAN::Optional<uint32_t> Terminal::get_8bit_color()
{
	ASSERT(m_csi_info.fields[1] == 5);
	if (m_csi_info.fields[2] < 1)
		return {};

	const uint8_t code = BAN::Math::min(m_csi_info.fields[2], 256) - 1;
	if (code < 8)
		return s_colors_dark[code];
	if (code < 16)
		return s_colors_bright[code - 8];

	if (code < 232)
	{
		const uint8_t r = (code - 16) / 36 % 6 * 40 + 55;
		const uint8_t g = (code - 16) /  6 % 6 * 40 + 55;
		const uint8_t b = (code - 16) /  1 % 6 * 40 + 55;
		return b | (g << 8) | (r << 16) | (0xCC << 24);
	}

	const uint8_t gray = (code - 232) * 10 + 8;
	return gray | (gray << 8) | (gray << 16) | (0xCC << 24);
}

BAN::Optional<uint32_t> Terminal::get_24bit_color()
{
	ASSERT(m_csi_info.fields[1] == 2);
	if (m_csi_info.fields[2] < 1) return {};
	if (m_csi_info.fields[3] < 1) return {};
	if (m_csi_info.fields[4] < 1) return {};
	const uint8_t r = BAN::Math::min(m_csi_info.fields[2], 256) - 1;
	const uint8_t g = BAN::Math::min(m_csi_info.fields[3], 256) - 1;
	const uint8_t b = BAN::Math::min(m_csi_info.fields[4], 256) - 1;
	return b | (g << 8) | (r << 16) | (0xCC << 24);
}

Rectangle Terminal::handle_csi(char ch)
{
	if (ch == ';')
	{
		m_csi_info.index++;
		return {};
	}

	if (ch == '?')
	{
		m_csi_info.question = true;
		return {};
	}

	if (isdigit(ch))
	{
		if (m_csi_info.index < m_csi_info.max_fields)
		{
			auto& field = m_csi_info.fields[m_csi_info.index];
			field = (BAN::Math::max(field, 0) * 10) + (ch - '0');
		}
		return {};
	}

	Rectangle should_invalidate;
	switch (ch)
	{
		case 'A':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::max<int32_t>(m_cursor.y - m_csi_info.fields[0], 0);
			break;
		case 'B':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::min<int32_t>(m_cursor.y + m_csi_info.fields[0], rows() - 1);
			break;
		case 'C':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::min<int32_t>(m_cursor.x + m_csi_info.fields[0], cols() - 1);
			break;
		case 'D':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::max<int32_t>(m_cursor.x - m_csi_info.fields[0], 0);
			break;
		case 'G':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, cols()) - 1;
			break;
		case 'H':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[1], 1, cols()) - 1;
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			break;
		case 'J':
		{
			Rectangle rects[2];
			size_t rect_count = 0;

			if (m_csi_info.fields[0] == -1 || m_csi_info.fields[0] == 0)
			{
				rects[0].x      = m_cursor.x * m_font.width();
				rects[0].y      = m_cursor.y * m_font.height();
				rects[0].width  = m_window->width() - rects[0].x;
				rects[0].height = m_font.height();

				rects[1].x      = 0;
				rects[1].y      = (m_cursor.y + 1) * m_font.height();
				rects[1].width  = m_window->width();
				rects[1].height = m_window->height() - rects[1].y;

				rect_count = 2;
			}
			else if (m_csi_info.fields[0] == 1)
			{
				rects[0].x      = 0;
				rects[0].y      = m_cursor.y * m_font.height();
				rects[0].width  = m_cursor.x * m_font.width();
				rects[0].height = m_font.height();

				rects[1].x      = 0;
				rects[1].y      = 0;
				rects[1].width  = m_window->width();
				rects[1].height = m_cursor.y * m_font.height();

				rect_count = 2;
			}
			else
			{
				rects[0].x      = 0;
				rects[0].y      = 0;
				rects[0].width  = m_window->width();
				rects[0].height = m_window->height();

				rect_count = 1;
			}

			for (size_t i = 0; i < rect_count; i++)
			{
				m_window->fill_rect(rects[i].x, rects[i].y, rects[i].width, rects[i].height, m_bg_color);
				should_invalidate = should_invalidate.get_bounding_box(rects[i]);
			}

			break;
		}
		case 'K':
		{
			m_csi_info.fields[0] = BAN::Math::max(m_csi_info.fields[0], 0);

			Rectangle rect;
			rect.x      = (m_csi_info.fields[0] == 0) ? m_cursor.x * m_font.width() : 0;
			rect.y      = m_cursor.y * m_font.height();
			rect.width  = (m_csi_info.fields[0] == 1) ? m_cursor.x * m_font.width() : m_window->width() - rect.x;
			rect.height = m_font.height();

			m_window->fill_rect(rect.x, rect.y, rect.width, rect.height, m_bg_color);
			should_invalidate = rect;

			break;
		}
		case 'L':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t src_y = m_cursor.y * m_font.height();
			const uint32_t dst_y = src_y + count * m_font.height();

			m_window->copy_horizontal_slice(dst_y, src_y, m_window->height() - dst_y, m_bg_color);
			m_window->fill_rect(0, src_y, m_window->width(), count * m_font.height(), m_bg_color);
			should_invalidate = {
				0,
				src_y,
				m_window->width(),
				m_window->height() - src_y
			};

			break;
		}
		case 'M':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t dst_y = m_cursor.y * m_font.height();
			const uint32_t src_y = dst_y + count * m_font.height();

			m_window->copy_horizontal_slice(dst_y, src_y, m_window->height() - dst_y, m_bg_color);
			m_window->fill_rect(0, m_window->height() - count * m_font.height(), m_window->width(), count * m_font.height(), m_bg_color);
			should_invalidate = {
				0,
				src_y,
				m_window->width(),
				m_window->height() - src_y
			};

			break;
		}
		case '@':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t dst_x = (m_cursor.x + count) * m_font.width();
			const uint32_t src_x = m_cursor.x * m_font.width();
			const uint32_t y = m_cursor.y * m_font.height();

			m_window->copy_rect(dst_x, y, src_x, y, m_window->width() - dst_x, m_font.height(), m_bg_color);
			m_window->fill_rect(src_x, y, count * m_font.width(), m_font.height(), m_bg_color);
			should_invalidate = {
				src_x,
				y,
				m_window->width() - src_x,
				m_font.height()
			};

			break;
		}
		case 'b':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			if (m_last_graphic_char)
				for (int32_t i = 0; i < m_csi_info.fields[0]; i++)
					should_invalidate = should_invalidate.get_bounding_box(putcodepoint(m_last_graphic_char));
			break;
		case 'd':
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			break;
		case 'm':
			if (m_csi_info.fields[0] == 38 || m_csi_info.fields[0] == 48)
			{
				if (m_csi_info.fields[1] != 5 && m_csi_info.fields[1] != 2)
				{
					dprintln("unsupported ANSI SGR {}", m_csi_info.fields[1]);
					break;
				}
				const auto color = (m_csi_info.fields[1] == 5)
					? get_8bit_color()
					: get_24bit_color();
				if (color.has_value())
					(m_csi_info.fields[0] == 38 ? m_fg_color : m_bg_color) = *color;
				break;
			}
			for (size_t i = 0; i <= m_csi_info.index && i < m_csi_info.max_fields; i++)
				handle_sgr(m_csi_info.fields[i]);
			break;
		case 's':
			m_saved_cursor = m_cursor;
			break;
		case 'u':
			m_cursor = m_saved_cursor;
			break;
		case 'h':
		case 'l':
			if (!m_csi_info.question || m_csi_info.fields[0] != 25)
			{
				dprintln("unsupported ANSI CSI {}", ch);
				break;
			}
			m_cursor_shown = (ch == 'h');
			break;
		case 'n':
			if (m_csi_info.fields[0] != 6)
			{
				dprintln("unsupported ANSI CSI n");
				break;
			}
			char buffer[2 + 10 + 1 + 10 + 2];
			sprintf(buffer, "\e[%u;%uR", m_cursor.y + 1, m_cursor.x + 1);
			write(m_shell_info.pts_master, buffer, strlen(buffer));
			break;
		default:
			dprintln("TODO: CSI {}", ch);
			break;
	}

	m_state = State::Normal;
	return should_invalidate;
}

Rectangle Terminal::putcodepoint(uint32_t codepoint)
{
	Rectangle should_invalidate;

	switch (codepoint)
	{
		case '\e':
			m_state = State::ESC;
			break;
		case '\n':
			m_cursor.x = 0;
			m_cursor.y++;
			break;
		case '\r':
			m_cursor.x = 0;
			break;
		case '\b':
			if (m_cursor.x > 0)
				m_cursor.x--;
			break;
		default:
		{
			if (m_cursor.x >= cols())
			{
				m_cursor.x = 0;
				m_cursor.y++;
			}

			if (m_cursor.y >= rows())
			{
				const uint32_t scroll = m_cursor.y - rows() + 1;
				m_cursor.y -= scroll;
				m_window->shift_vertical(-scroll * (int32_t)m_font.height(), m_bg_color);
				should_invalidate = { 0, 0, m_window->width(), m_window->height() };
			}

			const uint32_t cell_w = m_font.width();
			const uint32_t cell_h = m_font.height();
			const uint32_t cell_x = m_cursor.x * cell_w;
			const uint32_t cell_y = m_cursor.y * cell_h;

			const auto fg_color = m_colors_inverted ? m_bg_color : m_fg_color;
			const auto bg_color = m_colors_inverted ? m_fg_color : m_bg_color;

			m_window->fill_rect(cell_x, cell_y, cell_w, cell_h, bg_color);
			m_window->draw_character(codepoint, m_font, cell_x, cell_y, fg_color);
			m_last_graphic_char = codepoint;
			should_invalidate = { cell_x, cell_y, cell_w, cell_h };
			m_cursor.x++;
			break;
		}
	}

	if (m_cursor.y >= rows())
	{
		const uint32_t scroll = m_cursor.y - rows() + 1;
		m_cursor.y -= scroll;
		m_window->shift_vertical(-scroll * (int32_t)m_font.height(), m_bg_color);
		should_invalidate = { 0, 0, m_window->width(), m_window->height() };
	}

	return should_invalidate;
}

Rectangle Terminal::putchar(uint8_t ch)
{
	if (m_state == State::ESC)
	{
		if (ch != '[')
		{
			dprintln("unknown escape character 0x{2H}", ch);
			m_state = State::Normal;
			return {};
		}
		m_state = State::CSI;
		m_csi_info = {
			.fields = { -1, -1, -1, -1, -1 },
			.index = 0,
			.question = false,
		};
		return {};
	}

	if (m_state == State::CSI)
	{
		if (ch < 0x20 || ch > 0xFE)
		{
			dprintln("invalid CSI 0x{2H}", ch);
			m_state = State::Normal;
			return {};
		}
		return handle_csi(ch);
	}

	m_utf8_bytes[m_utf8_index++] = ch;

	const size_t utf8_len = BAN::UTF8::byte_length(m_utf8_bytes[0]);
	if (utf8_len == 0)
	{
		dwarnln("invalid utf8 leading byte 0x{2H}", ch);
		m_utf8_index = 0;
		return {};
	}
	if (m_utf8_index < utf8_len)
		return {};

	const uint32_t codepoint = BAN::UTF8::to_codepoint(m_utf8_bytes);
	m_utf8_index = 0;

	if (codepoint == BAN::UTF8::invalid)
	{
		char utf8_hex[20];
		char* ptr = utf8_hex;
		for (uint8_t i = 0; i < utf8_len; i++)
		{
			*ptr++ = '0';
			*ptr++ = 'x';
			*ptr++ = (m_utf8_bytes[i] >>  4) < 10 ? (m_utf8_bytes[i] >>  4) + '0' : (m_utf8_bytes[i] >>  4) - 10 + 'A';
			*ptr++ = (m_utf8_bytes[i] & 0xF) < 10 ? (m_utf8_bytes[i] & 0xF) + '0' : (m_utf8_bytes[i] & 0xF) - 10 + 'A';
			*ptr++ = ' ';
		}
		*--ptr = '\0';

		dwarnln("invalid utf8 {}", utf8_hex);
		return {};
	}

	return putcodepoint(codepoint);
}

void Terminal::on_key_event(LibGUI::EventPacket::KeyEvent::event_t event)
{
	if (event.released())
		return;
	if (const char* text = LibInput::key_to_utf8_ansi(event.key, event.modifier))
		write(m_shell_info.pts_master, text, strlen(text));
	m_got_key_event = true;
}
