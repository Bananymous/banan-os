#include <BAN/Queue.h>
#include <BAN/Sort.h>
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

	constexpr bool operator==(Position other) const
	{
		return x == other.x && y == other.y;
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

	BAN::Vector<bool> checked;
	MUST(checked.resize(map.width * map.height, false));

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			const auto pos = Position { .x = x, .y = y };
			if (checked[map.width * pos.y + pos.x])
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

				if (checked[map.width * pos.y + pos.x])
					continue;
				checked[map.width * pos.y + pos.x] = true;

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

	BAN::Vector<bool> checked;
	MUST(checked.resize(map.width * map.height, false));

	i64 result = 0;

	for (u32 y = 0; y < map.height; y++)
	{
		for (u32 x = 0; x < map.width; x++)
		{
			const auto pos = Position { .x = x, .y = y };
			if (checked[map.width * pos.y + pos.x])
				continue;

			const char target = map.get(pos.x, pos.y);

			struct PerimeterEntry
			{
				Position pos;
				bool counted;
			};
			BAN::Vector<PerimeterEntry> perimiters[4];

			struct CheckEntry
			{
				Position pos;
				uint8_t dir;
			};
			BAN::Queue<CheckEntry> checking;
			MUST(checking.push({ .pos = pos, .dir = 0 }));

			u32 area = 0;

			while (!checking.empty())
			{
				const auto [pos, dir] = checking.front();
				checking.pop();

				if (pos.x >= map.width || pos.y >= map.height || map.get(pos.x, pos.y) != target)
				{
					MUST(perimiters[dir].emplace_back(pos, true));
					continue;
				}

				if (checked[map.width * pos.y + pos.x])
					continue;
				checked[map.width * pos.y + pos.x] = true;

				area++;

				MUST(checking.push({ .pos { .x = pos.x + 1, .y = pos.y + 0 }, .dir = 0 }));
				MUST(checking.push({ .pos { .x = pos.x - 1, .y = pos.y + 0 }, .dir = 1 }));
				MUST(checking.push({ .pos { .x = pos.x + 0, .y = pos.y + 1 }, .dir = 2 }));
				MUST(checking.push({ .pos { .x = pos.x + 0, .y = pos.y - 1 }, .dir = 3 }));
			}

			for (auto& perimiter : perimiters)
			{
				BAN::sort::sort(perimiter.begin(), perimiter.end(),
					[](const auto& a, const auto& b) -> bool
					{
						if (a.pos.x != b.pos.x)
							return a.pos.x < b.pos.x;
						return a.pos.y < b.pos.y;
					}
				);

				for (auto it1 = perimiter.begin(); it1 != perimiter.end(); it1++)
				{
					for (auto it2 = it1 + 1; it2 != perimiter.end(); it2++)
					{
						if (it1->pos.x != it2->pos.x && it1->pos.y != it2->pos.y)
							continue;
						if (!it2->counted)
							continue;
						const u32 diff_x = it2->pos.x - it1->pos.x;
						const u32 diff_y = it2->pos.y - it1->pos.y;
						if ((diff_x == 0 && diff_y == 1) || (diff_x == 1 && diff_y == 0))
							it2->counted = false;
					}
				}
			}

			u32 sides = 0;
			for (const auto& perimiter : perimiters)
				for (auto [_, counted] : perimiter)
					sides += counted;

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
