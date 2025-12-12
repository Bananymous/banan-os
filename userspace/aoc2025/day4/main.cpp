#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using Grid = BAN::Vector<BAN::Vector<bool>>;

static Grid parse_grid(FILE* fp)
{
	Grid grid;
	MUST(grid.emplace_back());

	char ch;
	while ((ch = fgetc(fp)) != EOF)
	{
		switch (ch)
		{
			case '.':
			case '@':
				MUST(grid.back().push_back(ch == '@'));
				break;
			case '\n':
				MUST(grid.emplace_back());
				break;
		}
	}

	while (grid.back().empty())
		grid.pop_back();

	return grid;
}

static i32 count_neighborgs(const Grid& grid, size_t x, size_t y)
{
	i32 count = 0;
	for (i32 yoff = -1; yoff <= 1; yoff++)
	{
		for (i32 xoff = -1; xoff <= 1; xoff++)
		{
			if (yoff == 0 && xoff == 0)
				continue;
			if (y + yoff >= grid.size())
				continue;
			if (x + xoff >= grid[y + yoff].size())
				continue;
			count += grid[y + yoff][x + xoff];
		}
	}
	return count;
}

i64 part1(FILE* fp)
{
	i32 result = 0;

	auto grid = parse_grid(fp);
	for (size_t y = 0; y < grid.size(); y++)
		for (size_t x = 0; x < grid[y].size(); x++)
			if (grid[y][x] && count_neighborgs(grid, x, y) < 4)
				result++;

	return result;
}

i64 part2(FILE* fp)
{
	i32 result = 0;

	auto grid = parse_grid(fp);

	for (;;)
	{
		const auto old_result = result;

		for (size_t y = 0; y < grid.size(); y++)
		{
			for (size_t x = 0; x < grid[y].size(); x++)
			{
				if (!grid[y][x] || count_neighborgs(grid, x, y) >= 4)
					continue;
				grid[y][x] = false;
				result++;
			}
		}

		if (result == old_result)
			break;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day4_input.txt";

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
