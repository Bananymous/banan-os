#include <BAN/Math.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <termios.h>

#include <LibInput/MouseEvent.h>

framebuffer_info_t fb_info;
void* fb_mmap = nullptr;

int mouse_fd = -1;

termios original_termios {};

void draw_circle(int cx, int cy, int r, uint32_t color)
{
	int min_x = BAN::Math::max<int>(cx - r, 0);
	int max_x = BAN::Math::min<int>(cx + r + 1, fb_info.width);

	int min_y = BAN::Math::max<int>(cy - r, 0);
	int max_y = BAN::Math::min<int>(cy + r + 1, fb_info.height);

	for (int y = min_y; y < max_y; y++)
	{
		for (int x = min_x; x < max_x; x++)
		{
			int dx = x - cx;
			int dy = y - cy;
			if (dx * dx + dy * dy > r * r)
				continue;
			static_cast<uint32_t*>(fb_mmap)[y * fb_info.width + x] = color;
		}
	}
}

void cleanup()
{
	if (fb_mmap)
		munmap(fb_mmap, fb_info.height * fb_info.width * (BANAN_FB_BPP / 8));
	if (mouse_fd != -1)
		close(mouse_fd);
	if (original_termios.c_lflag & ECHO)
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

int main(int argc, char** argv)
{
	const char* fb_path = "/dev/fb0";
	const char* mouse_path = "/dev/mouse0";

	if (argc == 1)
		;
	else if (argc == 3)
	{
		fb_path = argv[1];
		mouse_path = argv[2];
	}
	else
	{
		fprintf(stderr, "usage: %s [FB_PATH MOUSE_PATH]", argv[0]);
		return 1;
	}

	signal(SIGINT, [](int) { exit(0); });
	if (atexit(cleanup) == -1)
	{
		perror("atexit");
		return 1;
	}

	if (BANAN_FB_BPP != 32)
	{
		fprintf(stderr, "unsupported bpp\n");
		return 1;
	}

	int fb_fd = open(fb_path, O_RDWR);
	if (fb_fd == -1)
	{
		fprintf(stderr, "open: ");
		perror(fb_path);
		return 1;
	}

	if (pread(fb_fd, &fb_info, sizeof(fb_info), -1) == -1)
	{
		fprintf(stderr, "read: ");
		perror(fb_path);
		return 1;
	}

	size_t fb_bytes = fb_info.width * fb_info.height * (BANAN_FB_BPP / 8);
	fb_mmap = mmap(nullptr, fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	close(fb_fd);
	if (fb_mmap == MAP_FAILED)
	{
		fprintf(stderr, "mmap: ");
		perror(fb_path);
		return 1;
	}

	int mouse_fd = open(mouse_path, O_RDONLY);
	if (mouse_fd == -1)
	{
		fprintf(stderr, "open: ");
		perror(mouse_path);
		return 1;
	}

	if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
	{
		perror("tcgetattr");
		return 1;
	}

	termios termios = original_termios;
	termios.c_lflag &= ~ECHO;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &termios) == -1)
	{
		perror("tcsetattr");
		return 1;
	}

	uint32_t color = 0xFF0000;
	int mouse_x = fb_info.width / 2;
	int mouse_y = fb_info.height / 2;
	int radius = 10;

	// clear screen and render
	memset(fb_mmap, 0x00, fb_bytes);
	draw_circle(mouse_x, mouse_y, radius, color);
	msync(fb_mmap, fb_bytes, MS_SYNC);

	while (true)
	{
		using namespace LibInput;

		MouseEvent event;
		if (read(mouse_fd, &event, sizeof(event)) == -1)
		{
			fprintf(stderr, "read: ");
			perror(mouse_path);
			return 1;
		}

		switch (event.type)
		{
			case MouseEventType::MouseMoveEvent:
				draw_circle(mouse_x, mouse_y, radius, 0x000000);
				mouse_x = BAN::Math::clamp<int>(mouse_x + event.move_event.rel_x, 0, fb_info.width);
				mouse_y = BAN::Math::clamp<int>(mouse_y - event.move_event.rel_y, 0, fb_info.height);
				draw_circle(mouse_x, mouse_y, radius, color);
				break;
			case MouseEventType::MouseScrollEvent:
				draw_circle(mouse_x, mouse_y, radius, 0x000000);
				radius = BAN::Math::clamp(radius + event.scroll_event.scroll, 1, 50);
				draw_circle(mouse_x, mouse_y, radius, color);
				break;
			case MouseEventType::MouseButtonEvent:
				if (!event.button_event.pressed)
					break;
				switch (event.button_event.button)
				{
					case MouseButton::Left:		color = 0xFF0000; break;
					case MouseButton::Right:	color = 0x00FF00; break;
					case MouseButton::Middle:	color = 0x0000FF; break;
					case MouseButton::Extra1:	color = 0xFFFF00; break;
					case MouseButton::Extra2:	color = 0x00FFFF; break;
				}
				draw_circle(mouse_x, mouse_y, radius, color);
				break;
			default:
				break;
		}

		msync(fb_mmap, fb_bytes, MS_SYNC);
	}
}
