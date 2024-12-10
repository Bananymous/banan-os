#include <BAN/HashSet.h>
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

struct Position
{
	u32 x, y;

	constexpr bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}
};

struct PositionHash
{
	constexpr BAN::hash_t operator()(Position state) const
	{
		return BAN::hash<u64>{}((u64)state.x << 32 | (u64)state.y);
	}
};

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
};

static Grid2D read_grid2d(FILE* fp)
{
	usize width { 0 };
	usize height { 0 };
	BAN::Vector<char> data;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const usize len = strlen(buffer);
		if (len == 0 || buffer[0] == '\n')
			break;
		if (data.empty())
			width = len - 1;
		height++;

		ASSERT(buffer[width] == '\n');

		if (data.capacity() < height * width)
			MUST(data.reserve(2 * height * width));

		MUST(data.resize(height * width));
		memcpy(&data[(height - 1) * width], buffer, width);
	}

	(void)data.shrink_to_fit();

	return Grid2D {
		.width = width,
		.height = height,
		.data = BAN::move(data),
	};
}

i64 get_trailhead_score(const Grid2D& map, u32 x, u32 y, char curr, BAN::HashSet<Position, PositionHash>& trail_ends, bool allow_duplicate)
{
	if (x >= map.width)
		return 0;
	if (y >= map.height)
		return 0;

	if (map.get(x, y) != curr)
		return 0;
	if (curr == '9')
	{
		const auto pos = Position { .x = x, .y = y };
		if (trail_ends.contains(pos))
			return allow_duplicate;
		MUST(trail_ends.insert(Position { .x = x, .y = y }));
		return 1;
	}

	i64 result = 0;
	result += get_trailhead_score(map, x - 1, y,     curr + 1, trail_ends, allow_duplicate);
	result += get_trailhead_score(map, x,     y - 1, curr + 1, trail_ends, allow_duplicate);
	result += get_trailhead_score(map, x + 1, y,     curr + 1, trail_ends, allow_duplicate);
	result += get_trailhead_score(map, x,     y + 1, curr + 1, trail_ends, allow_duplicate);
	return result;
}

i64 part1(FILE* fp)
{
	auto map = read_grid2d(fp);

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			if (map.get(x, y) != '0')
				continue;
			BAN::HashSet<Position, PositionHash> trail_ends;
			result += get_trailhead_score(map, x, y, '0', trail_ends, false);
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto map = read_grid2d(fp);

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			if (map.get(x, y) != '0')
				continue;
			BAN::HashSet<Position, PositionHash> trail_ends;
			result += get_trailhead_score(map, x, y, '0', trail_ends, true);
		}
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day10_input.txt";

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
