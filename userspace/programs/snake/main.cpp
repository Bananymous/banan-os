#include <BAN/Vector.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

enum Direction
{
	None,
	Unknown,
	Left,
	Right,
	Up,
	Down,
};

struct Point
{
	int x, y;
	bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

bool				g_running		= true;
Point				g_grid_size		= { 21, 21 };
Direction			g_direction		= Direction::Up;
Point				g_head			= { g_grid_size.x / 2, g_grid_size.y / 2 };
size_t				g_tail_target	= 3;
int					g_score			= 0;
BAN::Vector<Point>	g_tail;
Point				g_apple;

Direction query_input()
{
	char c;
	if (read(STDIN_FILENO, &c, 1) != 1)
		return Direction::None;

	switch (c)
	{
		case 'w': case 'W':
			return Direction::Up;
		case 'a': case 'A':
			return Direction::Left;
		case 's': case 'S':
			return Direction::Down;
		case 'd': case 'D':
			return Direction::Right;
		default:
			return Direction::Unknown;
	}
}

const char* get_tail_char(Direction old_dir, Direction new_dir)
{
	const size_t old_idx = static_cast<size_t>(old_dir) - 2;
	const size_t new_idx = static_cast<size_t>(new_dir) - 2;

	// left, right, up, down
	constexpr const char* tail_char_map[4][4] {
		{ "═", "═", "╚", "╔" },
		{ "═", "═", "╝", "╗" },
		{ "╗", "╔", "║", "║" },
		{ "╝", "╚", "║", "║" },
	};

	return tail_char_map[old_idx][new_idx];
}

void set_grid_tile(Point point, const char* str, int off_x = 0)
{
	printf("\e[%d;%dH%s", (point.y + 1) + 1, (point.x + 1) * 2 + 1 + off_x, str);
}

__attribute__((format(printf, 1, 2)))
void print_score_line(const char* format, ...)
{
	printf("\e[%dH\e[m", g_grid_size.y + 3);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void update_apple()
{
	BAN::Vector<Point> free_tiles;
	for (int y = 0; y < g_grid_size.y; y++)
		for (int x = 0; x < g_grid_size.x; x++)
			if (const Point point { x, y }; g_head != point && !g_tail.contains(point))
				MUST(free_tiles.push_back(point));

	if (free_tiles.empty())
	{
		print_score_line("You won!\n");
		exit(0);
	}

	g_apple = free_tiles[rand() % free_tiles.size()];
	set_grid_tile(g_apple, "\e[31mO");
}

void setup_grid()
{
	// Move cursor to beginning and clear screen
	printf("\e[H\e[2J");

	// Render top line
	printf("╔═");
	for (int x = 0; x < g_grid_size.x; x++)
		printf("══");
	printf("╗\n");

	// Render side lines
	for (int y = 0; y < g_grid_size.y; y++)
		printf("║\e[%dC║\n", g_grid_size.x * 2 + 1);

	// Render Bottom line
	printf("╚═");
	for (int x = 0; x < g_grid_size.x; x++)
		printf("══");
	printf("╝");

	// Render snake head
	printf("\e[32m");
	set_grid_tile(g_head, "O");

	// Generate and render apple
	srand(time(0));
	update_apple();

	// Render score
	print_score_line("Score: %d", g_score);

	fflush(stdout);
}

void update()
{
	auto input = Direction::None;
	auto new_direction = Direction::None;
	while ((input = query_input()) != Direction::None)
	{
		switch (input)
		{
			case Direction::Up:
				if (g_direction != Direction::Down)
					new_direction = Direction::Up;
				break;
			case Direction::Down:
				if (g_direction != Direction::Up)
					new_direction = Direction::Down;
				break;
			case Direction::Left:
				if (g_direction != Direction::Right)
					new_direction = Direction::Left;
				break;
			case Direction::Right:
				if (g_direction != Direction::Left)
					new_direction = Direction::Right;
				break;
			default:
				break;
		}
	}

	const auto old_direction = g_direction;
	if (new_direction != g_direction && new_direction != Direction::None)
		g_direction = new_direction;

	auto old_head = g_head;
	switch (g_direction)
	{
		case Direction::Up:
			g_head.y--;
			break;
		case Direction::Down:
			g_head.y++;
			break;
		case Direction::Left:
			g_head.x--;
			break;
		case Direction::Right:
			g_head.x++;
			break;
		default:
			ASSERT_NOT_REACHED();
	}

	if (g_head.x < 0 || g_head.y < 0 || g_head.x >= g_grid_size.x || g_head.y >= g_grid_size.y)
	{
		g_running = false;
		return;
	}

	for (auto point : g_tail)
	{
		if (point == g_head)
		{
			g_running = false;
			return;
		}
	}

	MUST(g_tail.insert(0, old_head));
	if (g_tail.size() > g_tail_target)
	{
		const auto comp = g_tail.size() >= 2 ? g_tail[g_tail.size() - 2] : g_head;
		const auto back = g_tail.back();

		if (comp.y == back.y)
		{
			if (comp.x == back.x + 1)
				set_grid_tile(back, " ", +1);
			if (comp.x == back.x - 1)
				set_grid_tile(back, " ", -1);
		}

		set_grid_tile(back, " ");
		g_tail.pop_back();
	}

	if (g_head == g_apple)
	{
		g_tail_target++;
		g_score++;
		update_apple();
		print_score_line("Score: %d", g_score);
	}

	printf("\e[32m");
	if (g_direction == Direction::Left)
		set_grid_tile(g_head, "═", +1);
	if (g_direction == Direction::Right)
		set_grid_tile(g_head, "═", -1);
	set_grid_tile(old_head, get_tail_char(old_direction, g_direction));
	set_grid_tile(g_head, "O");
	fflush(stdout);
}

int main()
{
	// Make stdin non blocking
	if (fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK))
	{
		perror("fcntl");
		return 1;
	}

	// Set stdin mode to non-canonical
	termios tcold, tcnew;
	if (tcgetattr(STDIN_FILENO, &tcold) == -1)
	{
		perror("tcgetattr");
		return 1;
	}

	tcnew = tcold;
	tcnew.c_lflag &= ~(ECHO | ICANON);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tcnew))
	{
		perror("tcsetattr");
		return 1;
	}

	printf("\e[?25l");
	setup_grid();

	timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 100'000'000;

	while (g_running)
	{
		nanosleep(&delay, nullptr);
		update();
	}

	// Restore stdin mode
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tcold))
	{
		perror("tcsetattr");
		return 1;
	}

	// Reset ansi state
	printf("\e[m\e[?25h\e[%dH", g_grid_size.y + 4);

	return 0;
}
