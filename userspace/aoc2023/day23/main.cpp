#include <BAN/Array.h>
#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <ctype.h>
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

BAN::Vector<BAN::Vector<char>> parse_grid(FILE* fp)
{
	BAN::Vector<BAN::Vector<char>> grid;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		MUST(grid.emplace_back(line.size(), '\0'));
		for (size_t i = 0; i < line.size(); i++)
			grid.back()[i] = line[i];
	}

	return grid;
}

struct Position
{
	i64 x, y;

	Position operator+(const Position& other) const
	{
		return { .x = x + other.x, .y = y + other.y };
	}

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}
};

i64 recurse_grid(BAN::Vector<Position>& path, const BAN::Vector<BAN::Vector<char>>& grid)
{
	const auto entry = path.back();
	BAN::ScopeGuard _([&] { while (path.back() != entry) path.pop_back(); });

	while (path.back().y < (i64)grid.size() - 1)
	{
		BAN::Array<Position, 4> valid_next;
		size_t valid_next_count = 0;

		constexpr Position offsets[4] { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		for (size_t i = 0; i < 4; i++)
		{
			auto next = path.back() + offsets[i];
			if (next.y < 0 || next.y >= (i64)grid.size())
				continue;
			if (next.x < 0 || next.x >= (i64)grid.front().size())
				continue;
			switch (grid[next.y][next.x])
			{
				case '^': next.y--; break;
				case 'v': next.y++; break;
				case '<': next.x--; break;
				case '>': next.x++; break;
				case '#': continue;
			}
			if (path.contains(next))
				continue;
			valid_next[valid_next_count++] = next;
		}

		if (valid_next_count == 0)
			return 0;

		if (valid_next_count == 1)
		{
			MUST(path.push_back(valid_next.front()));
			continue;
		}

		i64 result = 0;
		for (size_t i = 0; i < valid_next_count; i++)
		{
			MUST(path.push_back(valid_next[i]));
			result = BAN::Math::max(result, recurse_grid(path, grid));
			path.pop_back();
		}
		return result;
	}

	i64 result = 0;
	for (size_t i = 1; i < path.size(); i++)
		result += BAN::Math::abs(path[i - 1].x - path[i].x) + BAN::Math::abs(path[i - 1].y - path[i].y);
	return result;
}

i64 puzzle1(FILE* fp)
{
	auto grid = parse_grid(fp);

	BAN::Vector<Position> path;
	for (i64 x = 0; x < (i64)grid.front().size(); x++)
	{
		if (grid.front()[x] == '.')
		{
			MUST(path.push_back({ .x = x, .y = 0 }));
			break;
		}
	}

	return recurse_grid(path, grid);
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day23_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %" PRId64 "\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
