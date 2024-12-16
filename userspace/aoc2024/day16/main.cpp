#include <BAN/Array.h>
#include <BAN/CircularQueue.h>
#include <BAN/HashSet.h>
#include <BAN/UniqPtr.h>
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

template<typename T>
struct Vec2
{
	T x, y;

	constexpr bool operator==(const Vec2& other) const
	{
		return x == other.x && y == other.y;
	}
};

template<typename T>
struct Vec2Hash
{
	constexpr BAN::hash_t operator()(const Vec2<T> val) const
	{
		return BAN::hash<T>()(val.x) ^ BAN::hash<T>()(val.y);
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

	Vec2<usize> start, end;
	for (usize y = 0; y < grid.height; y++)
	{
		for (usize x = 0; x < grid.width; x++)
		{
			if (grid.get(x, y) == 'S')
				start = { x, y };
			if (grid.get(x, y) == 'E')
				end = { x, y };
		}
	}

	return ParseInputResult {
		.grid = BAN::move(grid),
		.start = start,
		.end = end,
	};
}

constexpr Vec2<isize> dirs[4] {
	{  1,  0 },
	{  0,  1 },
	{ -1,  0 },
	{  0, -1 },
};

static BAN::Vector<BAN::Array<u32, 4>> score_each_tile(const Grid2D& grid, Vec2<usize> start)
{
	BAN::Vector<BAN::Array<u32, 4>> cheapest;
	MUST(cheapest.resize(grid.data.size(), 0xFFFFFFFF));

	struct State
	{
		Vec2<usize> pos;
		u8 dir;
	};
	auto pending = MUST(BAN::UniqPtr<BAN::CircularQueue<State, 100'000>>::create());

	pending->push({ .pos = start, .dir = 0 });
	cheapest[start.y * grid.width + start.x][0] = 0;

	while (!pending->empty())
	{
		const auto [pos, dir] = pending->front();
		pending->pop();

		if (grid.get(pos.x, pos.y) == '#')
			continue;

		const usize idx = pos.y * grid.width + pos.x;
		const u32 points = cheapest[idx][dir];

		for (i32 i = 1; i <= 3; i += 2)
		{
			const usize next_idx  = idx;
			const u32 next_points = points + 1000;
			const u8  next_dir    = (dir + i) % 4;
			if (next_points >= cheapest[next_idx][next_dir])
				continue;
			cheapest[next_idx][next_dir] = next_points;
			pending->push({ .pos = pos, .dir = next_dir });
		}

		const auto next_pos = Vec2<usize> {
			.x = pos.x + dirs[dir].x,
			.y = pos.y + dirs[dir].y,
		};
		const u32 next_points = points + 1;

		const usize next_idx = next_pos.y * grid.width + next_pos.x;
		if (next_points >= cheapest[next_idx][dir])
			continue;

		cheapest[next_idx][dir] = next_points;
		pending->push({ .pos = next_pos, .dir = dir });
	}

	return cheapest;
}

i64 part1(FILE* fp)
{
	const auto [grid, start, end] = parse_input(fp);
	const auto cheapest = score_each_tile(grid, start);

	u32 result = 0xFFFFFFFF;
	for (u32 val : cheapest[end.y * grid.width + end.x])
		result = BAN::Math::min(result, val);
	return result;
}

i64 part2(FILE* fp)
{
	const auto [grid, start, end] = parse_input(fp);
	const auto cheapest = score_each_tile(grid, start);

	BAN::HashSet<Vec2<usize>, Vec2Hash<usize>> on_cheapest;

	struct State
	{
		Vec2<usize> pos;
		u8 dir;
	};
	BAN::Vector<State> pending;

	u32 min_cost = 0xFFFFFFFF;
	for (u32 val : cheapest[end.y * grid.width + end.x])
		min_cost = BAN::Math::min(min_cost, val);

	for (uint8_t dir = 0; dir < 4; dir++)
		if (cheapest[end.y * grid.width + end.x][dir] == min_cost)
			MUST(pending.push_back({ .pos = end, .dir = dir }));

	while (!pending.empty())
	{
		const auto [pos, dir] = pending.front();
		pending.remove(0);

		if (grid.get(pos.x, pos.y) == '#')
			continue;
		MUST(on_cheapest.insert(pos));

		if (pos == start)
			continue;

		const u32 points = cheapest[pos.y * grid.width + pos.x][dir];

		for (i32 i = 1; i <= 3; i += 2)
		{
			const usize idx = pos.y * grid.width + pos.x;
			const u32 prev_points = points - 1000;
			const u8  prev_dir    = (dir + i + 2) % 4;
			if (prev_points != cheapest[idx][prev_dir])
				continue;
			MUST(pending.push_back({ .pos = pos, .dir = prev_dir }));
		}

		const auto prev_pos = Vec2<usize> {
			.x = pos.x - dirs[dir].x,
			.y = pos.y - dirs[dir].y,
		};
		const u32 prev_points = points - 1;

		const usize prev_idx = prev_pos.y * grid.width + prev_pos.x;
		if (prev_points != cheapest[prev_idx][dir])
			continue;
		MUST(pending.push_back({ .pos = prev_pos, .dir = dir }));
	}

	return on_cheapest.size();
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day16_input.txt";

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
