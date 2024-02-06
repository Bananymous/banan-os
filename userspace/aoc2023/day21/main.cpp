#include <BAN/Vector.h>
#include <BAN/HashSet.h>

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

enum class Tile { Free, Rock };

struct Position
{
	i64 x;
	i64 y;

	Position operator+(const Position& other) const
	{
		return {
			.x = x + other.x,
			.y = y + other.y
		};
	}

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}
};

namespace BAN
{
	template<>
	struct hash<Position>
	{
		hash_t operator()(const Position& position) const
		{
			return hash<u64>()(((u64)position.x << 32) | (u64)position.y);
		}
	};
}

struct Garden
{
	Position start;
	BAN::Vector<BAN::Vector<Tile>> tiles;
};

Garden parse_garden(FILE* fp)
{
	Garden garden;

	i64 row = 0;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		MUST(garden.tiles.emplace_back(line.size(), Tile::Free));
		for (size_t i = 0; i < line.size(); i++)
		{
			if (line[i] == '#')
				garden.tiles.back()[i] = Tile::Rock;
			if (line[i] == 'S')
				garden.start = { .x = (i64)i, .y = row };
		}

		row++;
	}

	return garden;
}

i64 puzzle1(FILE* fp)
{
	auto garden = parse_garden(fp);

	BAN::HashSet<Position> visited, reachable, pending;
	MUST(pending.insert(garden.start));

	for (i32 i = 0; i <= 64; i++)
	{
		auto temp = BAN::move(pending);
		pending = BAN::HashSet<Position>();

		while (!temp.empty())
		{
			auto position = *temp.begin();
			temp.remove(position);

			MUST(visited.insert(position));
			if (i % 2 == 0)
				MUST(reachable.insert(position));

			Position offsets[4] { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
			for (i32 j = 0; j < 4; j++)
			{
				auto next = position + offsets[j];
				if (next.y < 0 || next.y >= (i64)garden.tiles.size())
					continue;
				if (next.x < 0 || next.x >= (i64)garden.tiles[0].size())
					continue;
				if (garden.tiles[next.y][next.x] == Tile::Rock)
					continue;
				if (visited.contains(next))
					continue;
				MUST(pending.insert(next));
			}
		}
	}

	return reachable.size();
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day21_input.txt";

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
