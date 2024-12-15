#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = ssize_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

struct Grid2D
{
	usize width { 0 };
	usize height { 0 };
	BAN::Vector<char> data;

	inline char get(usize x, usize y) const
	{
		ASSERT(x < width && y < height);
		return data[y * width + x];
	}

	inline char& get(usize x, usize y)
	{
		ASSERT(x < width && y < height);
		return data[y * width + x];
	}
};

struct Position
{
	i32 x, y;
};

enum class Dir
{
	Up, Down,
	Left, Right,
};

struct ParseInputResult
{
	Grid2D grid;
	Position robot;
	BAN::Vector<Dir> moves;
};

static ParseInputResult parse_input(FILE* fp, bool wide)
{
	Grid2D grid;

	char buffer[1024] {};
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const usize len = strlen(buffer);
		if (len == 0 || buffer[0] == '\n')
			break;
		if (grid.data.empty())
			grid.width = len - 1;
		grid.height++;

		ASSERT(buffer[grid.width] == '\n');

		if (grid.data.capacity() < grid.height * grid.width)
			MUST(grid.data.reserve(2 * grid.height * grid.width));

		MUST(grid.data.resize(grid.height * grid.width));
		memcpy(&grid.data[(grid.height - 1) * grid.width], buffer, grid.width);
	}

	if (wide)
	{
		BAN::Vector<char> wide_data;
		MUST(wide_data.resize(grid.data.size() * 2));

		for (usize i = 0; i < grid.data.size(); i++)
		{
			char l = 0, r = 0;
			switch (grid.data[i])
			{
				case '#': l = '#'; r = '#'; break;
				case 'O': l = '['; r = ']'; break;
				case '.': l = '.'; r = '.'; break;
				case '@': l = '@'; r = '.'; break;
			}
			wide_data[i * 2 + 0] = l;
			wide_data[i * 2 + 1] = r;
		}

		grid.data = BAN::move(wide_data);
		grid.width *= 2;
	}

	constexpr auto char_to_dir =
		[](char ch) -> Dir
		{
			switch (ch)
			{
				case '^': return Dir::Up;
				case 'v': return Dir::Down;
				case '<': return Dir::Left;
				case '>': return Dir::Right;
			}
			ASSERT_NOT_REACHED();
		};

	BAN::Vector<Dir> moves;
	for (;;)
	{
		usize nread = fread(buffer, 1, sizeof(buffer), fp);
		if (nread == 0)
			break;
		MUST(moves.reserve(moves.size() + nread));
		for (usize i = 0; i < nread; i++)
			if (buffer[i] != '\n')
				MUST(moves.push_back(char_to_dir(buffer[i])));
	}

	Position robot;
	for (u32 y = 0; y < grid.height; y++)
	{
		for (u32 x = 0; x < grid.width; x++)
		{
			if (grid.data[y * grid.width + x] != '@')
				continue;
			grid.data[y * grid.width + x] = '.';
			robot = { (i32)x, (i32)y };
		}
	}

	return ParseInputResult {
		.grid = BAN::move(grid),
		.robot = robot,
		.moves = BAN::move(moves),
	};
}

i64 part1(FILE* fp)
{
	auto [grid, robot, moves] = parse_input(fp, false);

	for (usize i = 0; i < moves.size(); i++)
	{
#if 0
		printf("\e[H");
		for (usize y = 0; y < grid.height; y++) {
			for (usize x = 0; x < grid.width; x++)
				printf("%c ", grid.get(x, y));
			printf("\n");
		}
		printf("\e[%u;%uH\e[31m@\e[m", robot.y + 1, robot.x * 2 + 1);
		printf("\e[%zuH%zu/%zu\n", grid.height + 1, i, moves.size());

		getchar();
#endif

		i32 vx = 0, vy = 0;
		switch (moves[i])
		{
			case Dir::Up:    vx =  0; vy = -1; break;
			case Dir::Down:  vx =  0; vy =  1; break;
			case Dir::Left:  vx = -1; vy =  0; break;
			case Dir::Right: vx =  1; vy =  0; break;
		}

		Position empty { -1, -1 };
		for (usize j = 1;; j++)
		{
			const auto pos = Position {
				(i32)(robot.x + j * vx),
				(i32)(robot.y + j * vy),
			};

			const char ch = grid.get(pos.x, pos.y);
			if (ch == '#')
				break;
			if (ch == 'O')
				continue;
			ASSERT(ch == '.');
			empty = pos;
			break;
		}

		if (empty.x == -1 || empty.y == -1)
			continue;

		while (empty.x != robot.x || empty.y != robot.y)
		{
			const auto pos = Position {
				(i32)(empty.x - vx),
				(i32)(empty.y - vy),
			};
			grid.get(empty.x, empty.y) = grid.get(pos.x, pos.y);
			empty = pos;
		}
		grid.get(robot.x, robot.y) = '.';

		robot.x += vx;
		robot.y += vy;
	}

	i64 result = 0;
	for (usize y = 0; y < grid.height; y++)
		for (usize x = 0; x < grid.width; x++)
			if (grid.get(x, y) == 'O')
				result += 100 * y + x;

	return result;
}

static bool can_move(const Grid2D& grid, Position pos, Position dir)
{
	const char ch = grid.get(pos.x, pos.y);
	if (ch == '.')
		return true;
	if (ch == '#')
		return false;
	if (dir.x)
		return can_move(grid, { pos.x + dir.x, pos.y }, dir);

	ASSERT(ch == '[' || ch == ']');
	const i32 dir_x = (ch == '[') ? 1 : -1;
	return can_move(grid, { pos.x,         pos.y + dir.y }, dir)
		&& can_move(grid, { pos.x + dir_x, pos.y + dir.y }, dir);
}

static void do_move(Grid2D& grid, Position pos, Position dir)
{
	const char ch = grid.get(pos.x, pos.y);
	ASSERT(ch != '#');

	if (ch == '.')
		;
	else if (dir.x)
		do_move(grid, { pos.x + dir.x, pos.y }, dir);
	else
	{
		ASSERT(ch == '[' || ch == ']');
		const i32 dir_x = (ch == '[') ? 1 : -1;
		do_move(grid, { pos.x,         pos.y + dir.y }, dir);
		do_move(grid, { pos.x + dir_x, pos.y + dir.y }, dir);
	}

	grid.get(pos.x, pos.y) = grid.get(pos.x - dir.x, pos.y - dir.y);
	grid.get(pos.x - dir.x, pos.y - dir.y) = '.';

	//grid.get(pos.x + dir.x, pos.y + dir.y) = grid.get(pos.x, pos.y);
	//grid.get(pos.x, pos.y) = '.';
}

i64 part2(FILE* fp)
{
	auto [grid, robot, moves] = parse_input(fp, true);

	for (usize i = 0; i < moves.size(); i++)
	{
#if 0
		printf("\e[H");
		for (usize y = 0; y < grid.height; y++) {
			for (usize x = 0; x < grid.width; x++)
				printf("%c", grid.get(x, y));
			printf("\n");
		}
		printf("\e[%u;%uH\e[31m@\e[m", robot.y + 1, robot.x + 1);
		printf("\e[%zuH%zu/%zu\n", grid.height + 1, i, moves.size());

		getchar();
#endif

		Position dir {};
		switch (moves[i])
		{
			case Dir::Up:    dir = {  0, -1 }; break;
			case Dir::Down:  dir = {  0,  1 }; break;
			case Dir::Left:  dir = { -1,  0 }; break;
			case Dir::Right: dir = {  1,  0 }; break;
		}

		const auto next = Position {
			robot.x + dir.x,
			robot.y + dir.y,
		};
		if (!can_move(grid, next, dir))
			continue;
		do_move(grid, next, dir);
		robot = next;
	}

	i64 result = 0;
	for (usize y = 0; y < grid.height; y++)
		for (usize x = 0; x < grid.width; x++)
			if (grid.get(x, y) == '[')
				result += 100 * y + x;

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day15_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("part1: %" PRId64 "\n", part1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("part2: %" PRId64 "\n", part2(fp));

	fclose(fp);
}
