#include <BAN/Vector.h>

#include <fcntl.h>
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
Point				g_head			= { 10, 10 };
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

void set_grid_tile(Point point, const char* str)
{
	printf("\e[%d;%dH%s", (point.y + 1) + 1, (point.x + 1) * 2 + 1, str);
}

Point get_random_point()
{
	return { .x = rand() % g_grid_size.x, .y = rand() % g_grid_size.y };
}

void update_apple()
{
regenerate:
	g_apple = get_random_point();
	if (g_head == g_apple)
		goto regenerate;
	for (auto point : g_tail)
		if (point == g_apple)
			goto regenerate;
	set_grid_tile(g_apple, "\e[31mO");
}

void setup_grid()
{
	// Move cursor to beginning and clear screen
	printf("\e[H\e[J");

	// Render top line
	putchar('#');
	for (int x = 1; x < g_grid_size.x + 2; x++)
		printf(" #");
	putchar('\n');

	// Render side lines
	for (int y = 0; y < g_grid_size.y; y++)
		printf("#\e[%dC#\n", g_grid_size.x * 2 + 1);

	// Render Bottom line
	putchar('#');
	for (int x = 1; x < g_grid_size.x + 2; x++)
		printf(" #");
	putchar('\n');

	// Render snake head
	set_grid_tile(g_head, "O");

	// Generate and render apple
	srand(time(0));
	update_apple();

	// Render score
	printf("\e[%dH\e[mScore: %d", g_grid_size.y + 3, g_score);

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
		set_grid_tile(g_tail.back(), " ");
		g_tail.pop_back();
	}

	if (g_head == g_apple)
	{
		g_tail_target++;
		g_score++;
		update_apple();
		printf("\e[%dH\e[mScore: %d", g_grid_size.y + 3, g_score);
	}

	set_grid_tile(old_head, "\e[32mo");
	set_grid_tile(g_head,   "\e[32mO");

	fflush(stdout);
}

int main()
{
	// Make stdin non blocking
	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
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
