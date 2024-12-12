#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Vector.h>
#include <BAN/Queue.h>

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

	constexpr bool operator==(Position other) const
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

struct PosDir
{
	Position pos;
	u8 dir;

	constexpr bool operator==(PosDir other) const
	{
		return pos == other.pos && dir == other.dir;
	}
};

struct PosDirHash
{
	constexpr BAN::hash_t operator()(PosDir state) const
	{
		return PositionHash{}(state.pos) ^ BAN::hash<uint8_t>{}(state.dir);
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

	char buffer[1024] {};
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

i64 part1(FILE* fp)
{
	auto map = read_grid2d(fp);

	BAN::HashSet<Position, PositionHash> checked;

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			const auto pos = Position { .x = x, .y = y };
			if (checked.contains(pos))
				continue;

			const char target = map.get(pos.x, pos.y);

			u32 area = 0;
			u32 perimiter = 0;

			BAN::Queue<Position> checking;
			MUST(checking.push(pos));

			while (!checking.empty())
			{
				const auto pos = checking.front();
				checking.pop();

				if (pos.x >= map.width || pos.y >= map.height || map.get(pos.x, pos.y) != target)
				{
					perimiter++;
					continue;
				}

				if (checked.contains(pos))
					continue;
				MUST(checked.insert(pos));

				area++;

				MUST(checking.push({ .x = pos.x + 1, .y = pos.y + 0 }));
				MUST(checking.push({ .x = pos.x - 1, .y = pos.y + 0 }));
				MUST(checking.push({ .x = pos.x + 0, .y = pos.y + 1 }));
				MUST(checking.push({ .x = pos.x + 0, .y = pos.y - 1 }));
			}

			result += area * perimiter;
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto map = read_grid2d(fp);

	BAN::HashSet<Position, PositionHash> checked;

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			const auto pos = Position { .x = x, .y = y };
			if (checked.contains(pos))
				continue;

			const char target = map.get(pos.x, pos.y);

			u32 area = 0;
			BAN::HashMap<PosDir, bool, PosDirHash> perimiter;

			BAN::Queue<PosDir> checking;
			MUST(checking.push({ .pos = pos, .dir = 0 }));

			while (!checking.empty())
			{
				auto pos_dir = checking.front();
				checking.pop();

				if (pos_dir.pos.x >= map.width || pos_dir.pos.y >= map.height || map.get(pos_dir.pos.x, pos_dir.pos.y) != target)
				{
					MUST(perimiter.insert(pos_dir, true));
					continue;
				}

				if (checked.contains(pos_dir.pos))
					continue;
				MUST(checked.insert(pos_dir.pos));

				area++;

				const auto pos = pos_dir.pos;
				MUST(checking.push({ .pos { .x = pos.x + 1, .y = pos.y + 0 }, .dir = 0 }));
				MUST(checking.push({ .pos { .x = pos.x - 1, .y = pos.y + 0 }, .dir = 1 }));
				MUST(checking.push({ .pos { .x = pos.x + 0, .y = pos.y + 1 }, .dir = 2 }));
				MUST(checking.push({ .pos { .x = pos.x + 0, .y = pos.y - 1 }, .dir = 3 }));
			}

			for (auto it1 = perimiter.begin(); it1 != perimiter.end(); it1++)
			{
				for (auto it2 = BAN::next(it1, 1); it2 != perimiter.end(); it2++)
				{
					if (it1->key.dir != it2->key.dir)
						continue;
					if (it1->key.pos.x != it2->key.pos.x && it1->key.pos.y != it2->key.pos.y)
						continue;

					auto min_it = it1, max_it = it2;
					if (min_it->key.pos.x > max_it->key.pos.x || min_it->key.pos.y > max_it->key.pos.y)
						BAN::swap(min_it, max_it);

					if (!max_it->value)
						continue;

					const u32 diff_x = max_it->key.pos.x - min_it->key.pos.x;
					const u32 diff_y = max_it->key.pos.y - min_it->key.pos.y;
					if ((diff_x == 0 && diff_y == 1) || (diff_x == 1 && diff_y == 0))
						max_it->value = false;
				}
			}

			u32 sides = 0;
			for (auto [_, count] : perimiter)
				sides += count;

			result += area * sides;
		}
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day12_input.txt";

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
