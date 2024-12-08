#include <BAN/HashSet.h>
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

static BAN::Vector<BAN::Vector<char>> parse_input(FILE* fp)
{
	BAN::Vector<BAN::Vector<char>> result;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (!buffer[0] || buffer[0] == '\n')
			break;

		const size_t row_len = strlen(buffer) - 1;
		ASSERT(buffer[row_len] == '\n');

		if (!result.empty())
			ASSERT(result.back().size() == row_len);

		BAN::Vector<char> row;
		MUST(row.resize(row_len));
		memcpy(row.data(), buffer, row_len);

		MUST(result.push_back(BAN::move(row)));
	}

	return result;
}

i64 part1(FILE* fp)
{
	auto input = parse_input(fp);

	BAN::HashSet<Position, PositionHash> antinodes;

	for (size_t y1 = 0; y1 < input.size(); y1++)
	{
		for (size_t x1 = 0; x1 < input[y1].size(); x1++)
		{
			if (input[y1][x1] == '.')
				continue;

			for (size_t y2 = y1; y2 < input.size(); y2++)
			{
				for (size_t x2 = (y2 == y1) ? x1 + 1 : 0; x2 < input[y2].size(); x2++)
				{
					if (input[y2][x2] != input[y1][x1])
						continue;

					const size_t x_diff = x2 - x1;
					const size_t y_diff = y2 - y1;

					auto antinode1 = Position { .x = (u32)(x1 - x_diff), .y = (u32)(y1 - y_diff) };
					if (antinode1.y < input.size() && antinode1.x < input[antinode1.y].size())
						MUST(antinodes.insert(antinode1));

					auto antinode2 = Position { .x = (u32)(x2 + x_diff), .y = (u32)(y2 + y_diff) };
					if (antinode2.y < input.size() && antinode2.x < input[antinode2.y].size())
						MUST(antinodes.insert(antinode2));
				}
			}
		}
	}

	return antinodes.size();
}

i64 part2(FILE* fp)
{
	auto input = parse_input(fp);

	BAN::HashSet<Position, PositionHash> antinodes;

	for (size_t y1 = 0; y1 < input.size(); y1++)
	{
		for (size_t x1 = 0; x1 < input[y1].size(); x1++)
		{
			if (input[y1][x1] == '.')
				continue;

			for (size_t y2 = y1; y2 < input.size(); y2++)
			{
				for (size_t x2 = (y2 == y1) ? x1 + 1 : 0; x2 < input[y2].size(); x2++)
				{
					if (input[y2][x2] != input[y1][x1])
						continue;

					const size_t x_diff = x2 - x1;
					const size_t y_diff = y2 - y1;

					for (size_t t = 0;; t++)
					{
						auto antinode = Position { .x = (u32)(x1 - t * x_diff), .y = (u32)(y1 - t * y_diff) };
						if (antinode.y >= input.size() || antinode.x >= input[antinode.y].size())
							break;
						MUST(antinodes.insert(antinode));
					}

					for (size_t t = 0;; t++)
					{
						auto antinode = Position { .x = (u32)(x2 + t * x_diff), .y = (u32)(y2 + t * y_diff) };
						if (antinode.y >= input.size() || antinode.x >= input[antinode.y].size())
							break;
						MUST(antinodes.insert(antinode));
					}
				}
			}
		}
	}

	return antinodes.size();
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day8_input.txt";

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
