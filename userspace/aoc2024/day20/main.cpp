#include <BAN/Optional.h>
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

template<typename T>
struct Vec2
{
	T x, y;
};

struct Grid2D
{
	struct Tile
	{
		static constexpr u32 undefined_cost = BAN::numeric_limits<u32>::max();

		bool wall;
		u32 cost;
	};

	usize width { 0 };
	usize height { 0 };
	BAN::Vector<Tile> data;

	inline Tile get(Vec2<usize> pos) const
	{
		ASSERT(pos.x < width && pos.y < height);
		return data[pos.y * width + pos.x];
	}

	inline Tile& get(Vec2<usize> pos)
	{
		ASSERT(pos.x < width && pos.y < height);
		return data[pos.y * width + pos.x];
	}
};

struct ParseInputResult
{
	Grid2D grid;
	Vec2<usize> start;
	Vec2<usize> end;
};

static ParseInputResult parse_input(FILE* fp)
{
	Grid2D grid;
	Vec2<usize> start, end;

	char buffer[1024] {};
	for (usize y = 0; fgets(buffer, sizeof(buffer), fp); y++)
	{
		const usize len = strlen(buffer);
		if (len == 0 || buffer[0] == '\n')
			break;
		if (grid.data.empty())
			grid.width = len - 1;
		grid.height++;

		ASSERT(buffer[grid.width] == '\n');

		for (usize x = 0; x < grid.width; x++)
		{
			if (buffer[x] == 'S')
				start = { .x = x, .y = y };
			if (buffer[x] == 'E')
				end   = { .x = x, .y = y };
			MUST(grid.data.push_back({ .wall = (buffer[x] == '#'), .cost = Grid2D::Tile::undefined_cost }));
		}
	}

	return ParseInputResult {
		.grid = BAN::move(grid),
		.start = start,
		.end = end,
	};
}

static void evaluate_grid(Grid2D& grid, const Vec2<usize> start)
{
	struct ToCheck
	{
		Vec2<usize> pos;
		u32 cost;
	};
	BAN::Vector<ToCheck> to_check;
	MUST(to_check.push_back({ .pos = start, .cost = 0 }));

	while (!to_check.empty())
	{
		const auto [pos, cost] = to_check.front();
		to_check.remove(0);

		{
			auto& tile = grid.get(pos);
			if (tile.wall || tile.cost <= cost)
				continue;
			tile.cost = cost;
		}

		constexpr Vec2<isize> dirs[] {
			{  1,  0 },
			{ -1,  0 },
			{  0,  1 },
			{  0, -1 },
		};

		for (const auto dir : dirs)
		{
			const auto next_pos = Vec2<usize> {
				.x = pos.x + dir.x,
				.y = pos.y + dir.y,
			};
			if (next_pos.x >= grid.width || next_pos.y >= grid.height)
				continue;

			const auto next_tile = grid.get(next_pos);
			if (next_tile.wall || next_tile.cost <= cost + 1)
				continue;
			MUST(to_check.push_back({ .pos = next_pos, .cost = cost + 1}));
		}
	}
}

static u32 count_100_picosecond_cheats(const Grid2D& grid, i32 max_cheat_length)
{
	u32 result = 0;
	for (usize y = 0; y < grid.height; y++)
	{
		for (usize x = 0; x < grid.width; x++)
		{
			const auto tile = grid.get({ .x = x, .y = y });
			if (tile.wall || tile.cost == Grid2D::Tile::undefined_cost)
				continue;

			for (i32 yoff = -max_cheat_length; yoff <= max_cheat_length; yoff++)
			{
				const i32 max_xoff = max_cheat_length - BAN::Math::abs(yoff);
				for (i32 xoff = -max_xoff; xoff <= max_xoff; xoff++)
				{
					const auto next_pos = Vec2<usize> {
						.x = x + xoff,
						.y = y + yoff,
					};
					if (next_pos.x >= grid.width || next_pos.y >= grid.height)
						continue;

					const auto next_tile = grid.get(next_pos);
					if (next_tile.wall)
						continue;
					if (next_tile.cost == Grid2D::Tile::undefined_cost)
						continue;
					if (next_tile.cost <= tile.cost)
						continue;

					const usize distance = BAN::Math::abs(yoff) + BAN::Math::abs(xoff);
					const u32 save = next_tile.cost - tile.cost;
					if (save >= 100 + distance)
						result++;
				}
			}
		}
	}
	return result;
}

i64 part1(FILE* fp)
{
	auto [grid, start, end] = parse_input(fp);
	evaluate_grid(grid, start);
	return count_100_picosecond_cheats(grid, 2);
}

i64 part2(FILE* fp)
{
	auto [grid, start, end] = parse_input(fp);
	evaluate_grid(grid, start);
	return count_100_picosecond_cheats(grid, 20);
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day20_input.txt";

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
