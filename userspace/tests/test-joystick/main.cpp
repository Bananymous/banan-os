#include <BAN/Math.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <termios.h>

#include <LibInput/Joystick.h>

framebuffer_info_t fb_info;
void* fb_mmap = nullptr;

int joystick_fd = -1;

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
	if (joystick_fd != -1)
		close(joystick_fd);
	if (original_termios.c_lflag & ECHO)
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

int map_joystick(const LibInput::JoystickState::Axis& axis, float min, float max)
{
	if (axis.min == axis.max)
		return (min + max) / 2;
	return (axis.value - axis.min) * (max - min) / (axis.max - axis.min) + min;
}

int main(int argc, char** argv)
{
	const char* fb_path = "/dev/fb0";
	const char* joystick_path = "/dev/joystick0";

	if (argc == 1)
		;
	else if (argc == 3)
	{
		fb_path = argv[1];
		joystick_path = argv[2];
	}
	else
	{
		fprintf(stderr, "usage: %s [FB_PATH JOYSTICK_PATH]", argv[0]);
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

	joystick_fd = open(joystick_path, O_RDONLY);
	if (joystick_fd == -1)
	{
		fprintf(stderr, "open: ");
		perror(joystick_path);
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
	int circle_x = fb_info.width / 2;
	int circle_y = fb_info.height / 2;
	int radius = 10;

	// clear screen and render
	memset(fb_mmap, 0x00, fb_bytes);
	draw_circle(circle_x, circle_y, radius, color);
	msync(fb_mmap, fb_bytes, MS_SYNC);

	while (true)
	{
		LibInput::JoystickState state {};
		if (read(joystick_fd, &state, sizeof(state)) == -1)
		{
			fprintf(stderr, "read: ");
			perror(joystick_path);
			return 1;
		}

		const int dx = map_joystick(state.axis[0], -50, 50);
		const int dy = map_joystick(state.axis[1], -50, 50);
		const int dr = map_joystick(state.axis[3],   5, -5);

		draw_circle(circle_x, circle_y, radius, 0x000000);

		if (BAN::Math::abs(dx) >= 10)
			circle_x = BAN::Math::clamp<int>(circle_x + dx, 0, fb_info.width);
		if (BAN::Math::abs(dy) >= 10)
			circle_y = BAN::Math::clamp<int>(circle_y + dy, 0, fb_info.height);
		radius = BAN::Math::clamp<int>(radius + dr, 1, 100);

		if (state.buttons[12])
			color = 0xFF0000;
		if (state.buttons[13])
			color = 0x00FF00;
		if (state.buttons[14])
			color = 0x0000FF;
		if (state.buttons[15])
			color = 0xFFFFFF;

		draw_circle(circle_x, circle_y, radius, color);

		msync(fb_mmap, fb_bytes, MS_SYNC);

		usleep(16666);

		msync(fb_mmap, fb_bytes, MS_SYNC);
	}
}
