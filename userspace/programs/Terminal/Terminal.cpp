#include "Terminal.h"

#include <BAN/Debug.h>
#include <BAN/UTF8.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
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

	m_bg_color = s_colors_dark[0];
	m_fg_color = s_colors_bright[7];

	m_window = MUST(LibGUI::Window::create(600, 400, "Terminal"_sv));
	m_window->fill(m_bg_color);
	m_window->invalidate();

	m_font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv));

	{
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		m_cursor_blink_shown = true;
		m_cursor_blink_ms = ts.tv_sec * 1'000 + ts.tv_nsec / 1'000'000;
	}

	MUST(m_cursor_buffer.resize(m_font.width() * m_font.height(), m_bg_color));
	show_cursor();

	m_window->set_key_event_callback([&](LibGUI::EventPacket::KeyEvent event) { on_key_event(event); });

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
	const uint32_t cursor_base_x = m_cursor.x * m_font.width();
	const uint32_t cursor_base_y = m_cursor.y * m_font.height();
	for (uint32_t y = 0; y < m_font.height(); y++)
		for (uint32_t x = 0; x < m_font.width(); x++)
			m_window->set_pixel(cursor_base_x + x, cursor_base_y + y, m_cursor_buffer[y * m_font.width() + x]);
	m_window->invalidate(cursor_base_x, cursor_base_y, m_font.width(), m_font.height());
}

void Terminal::show_cursor()
{
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
		size_t non_ansi_end = i;
		while (non_ansi_end < nread && buffer[non_ansi_end] != '\e')
			non_ansi_end++;

		// we only need to process maximum of `rows()` newlines.
		// anything before that would get overwritten anyway
		size_t start = non_ansi_end;
		size_t newline_count = 0;
		while (start > i && newline_count < rows())
			newline_count += (buffer[--start] == '\n');

		// do possible scrolling already in here, so `putchar()` doesnt
		// have to scroll up to `rows()` times
		if (m_cursor.y + newline_count >= rows())
		{
			const uint32_t scroll = m_cursor.y + newline_count - rows() + 1;
			m_cursor.y -= scroll;
			m_window->shift_vertical(-scroll * (int32_t)m_font.height());
			m_window->fill_rect(0, m_window->height() - scroll * m_font.height(), m_window->width(), scroll * m_font.height(), m_bg_color);
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

void Terminal::handle_sgr()
{
	switch (m_csi_info.fields[0])
	{
		case -1: case 0:
			m_bg_color = s_colors_dark[0];
			m_fg_color = s_colors_bright[7];
			break;
		case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
			m_fg_color = s_colors_dark[m_csi_info.fields[0] - 30];
			break;
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
			m_bg_color = s_colors_dark[m_csi_info.fields[0] - 40];
			break;
		case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
			m_fg_color = s_colors_bright[m_csi_info.fields[0] - 90];
			break;
		case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
			m_bg_color = s_colors_bright[m_csi_info.fields[0] - 100];
			break;
		default:
			dprintln("TODO: SGR {}", m_csi_info.fields[0]);
			break;
	}
}

void Terminal::handle_csi(char ch)
{
	if (ch == ';')
	{
		m_csi_info.index++;
		return;
	}

	if (ch == '?')
	{
		m_csi_info.question = true;
		return;
	}

	if (isdigit(ch))
	{
		if (m_csi_info.index <= 1)
		{
			auto& field = m_csi_info.fields[m_csi_info.index];
			field = (BAN::Math::max(field, 0) * 10) + (ch - '0');
		}
		return;
	}

	switch (ch)
	{
		case 'C':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::clamp<int32_t>(m_cursor.x + m_csi_info.fields[0], 0, cols() - 1);
			break;
		case 'D':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::clamp<int32_t>((int32_t)m_cursor.x - m_csi_info.fields[0], 0, cols() - 1);
			break;
		case 'G':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, cols()) - 1;
			break;
		case 'H':
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[1], 1, cols()) - 1;
			break;
		case 'J':
		{
			uint32_t rects[2][4] { { (uint32_t)-1 }, { (uint32_t)-1 } };

			if (m_csi_info.fields[0] == -1 || m_csi_info.fields[0] == 0)
			{
				rects[0][0] = m_cursor.x * m_font.width();
				rects[0][1] = m_cursor.y * m_font.height();
				rects[0][2] = m_window->width() - rects[0][0];
				rects[0][3] = m_font.height();

				rects[1][0] = 0;
				rects[1][1] = (m_cursor.y + 1) * m_font.height();
				rects[1][2] = m_window->width();
				rects[1][3] = m_window->height() - rects[1][1];
			}
			else if (m_csi_info.fields[0] == 1)
			{
				rects[0][0] = 0;
				rects[0][1] = m_cursor.y * m_font.height();
				rects[0][2] = m_cursor.x * m_font.width();
				rects[0][3] = m_font.height();

				rects[1][0] = 0;
				rects[1][1] = 0;
				rects[1][2] = m_window->width();
				rects[1][3] = m_cursor.y * m_font.height();
			}
			else
			{
				rects[0][0] = 0;
				rects[0][1] = 0;
				rects[0][2] = m_window->width();
				rects[0][3] = m_window->height();
			}

			for (int i = 0; i < 2; i++)
			{
				if (rects[i][0] == (uint32_t)-1)
					continue;
				m_window->fill_rect(rects[i][0], rects[i][1], rects[i][2], rects[i][3], m_bg_color);
				m_window->invalidate(rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
			}
			break;
		}
		case 'K':
		{
			m_csi_info.fields[0] = BAN::Math::max(m_csi_info.fields[0], 0);

			uint32_t rect[4];
			rect[0] = (m_csi_info.fields[0] == 0) ? m_cursor.x * m_font.width() : 0;
			rect[1] = m_cursor.y * m_font.height();
			rect[2] = (m_csi_info.fields[0] == 1) ? m_cursor.x * m_font.width() : m_window->width() - rect[0];
			rect[3] = m_font.height();

			m_window->fill_rect(rect[0], rect[1], rect[2], rect[3], m_bg_color);
			m_window->invalidate(rect[0], rect[1], rect[2], rect[3]);

			break;
		}
		case 'm':
			handle_sgr();
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
				dprintln("invalid ANSI CSI ?{}{}", m_csi_info.fields[0], (char)ch);
				break;
			}
			m_cursor_shown = (ch == 'h');
			break;
		default:
			dprintln("TODO: CSI {}", ch);
			break;
	}
	m_state = State::Normal;
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
		m_csi_info.index = 0;
		m_csi_info.fields[0] = -1;
		m_csi_info.fields[1] = -1;
		m_csi_info.question = false;
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
		handle_csi(ch);
		return {};
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
			const uint32_t cell_w = m_font.width();
			const uint32_t cell_h = m_font.height();
			const uint32_t cell_x = m_cursor.x * cell_w;
			const uint32_t cell_y = m_cursor.y * cell_h;

			m_window->fill_rect(cell_x, cell_y, cell_w, cell_h, m_bg_color);
			m_window->draw_character(codepoint, m_font, cell_x, cell_y, m_fg_color);
			should_invalidate = { cell_x, cell_y, cell_w, cell_h };
			m_cursor.x++;
			break;
		}
	}

	if (m_cursor.x >= cols())
	{
		m_cursor.x = 0;
		m_cursor.y++;
	}

	if (m_cursor.y >= rows())
	{
		const uint32_t scroll = m_cursor.y - rows() + 1;
		m_cursor.y -= scroll;
		m_window->shift_vertical(-scroll * (int32_t)m_font.height());
		m_window->fill_rect(0, m_window->height() - scroll * m_font.height(), m_window->width(), scroll * m_font.height(), m_bg_color);
		should_invalidate = { 0, 0, m_window->width(), m_window->height() };
	}

	return should_invalidate;
}

void Terminal::on_key_event(LibGUI::EventPacket::KeyEvent event)
{
	if (event.released())
		return;
	if (const char* text = LibInput::key_to_utf8_ansi(event.key, event.modifier))
		write(m_shell_info.pts_master, text, strlen(text));
	m_got_key_event = true;
}
