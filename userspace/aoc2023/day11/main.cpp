#include <BAN/Swap.h>
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

using Map = BAN::Vector<BAN::Vector<u8>>;

Map read_map(FILE* fp)
{
	Map result;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const size_t len = strlen(buffer);
		if (len < 2)
			break;
		MUST(result.emplace_back(len - 1));
		for (size_t i = 0; i < len - 1; i++)
			result.back()[i] = buffer[i];
	}

	return result;
}

i64 distance_with_expansion(const Map& map, u64 x1, u64 y1, u64 x2, u64 y2, u64 expansion_size)
{
	ASSERT(map[y1][x1] == '#');
	ASSERT(map[y2][x2] == '#');

	if (x2 < x1) BAN::swap(x1, x2);
	if (y2 < y1) BAN::swap(y1, y2);

	u64 expansion_count = 0;

	for (u64 y = y1; y < y2; y++)
	{
		bool is_expanded = true;
		for (u64 x = 0; x < map[0].size() && is_expanded; x++)
			if (map[y][x] == '#')
				is_expanded = false;
		expansion_count += is_expanded;
	}

	for (u64 x = x1; x < x2; x++)
	{
		bool is_expanded = true;
		for (u64 y = 0; y < map.size() && is_expanded; y++)
			if (map[y][x] == '#')
				is_expanded = false;
		expansion_count += is_expanded;
	}

	return (x2 - x1) + (y2 - y1) + expansion_count * (expansion_size - 1);
}

i64 solution(FILE* fp, u64 expansion)
{
	auto map = read_map(fp);

	i64 result = 0;

	for (size_t i = 0; i < map.size() * map[0].size(); i++)
	{
		const u64 y1 = i / map[0].size();
		const u64 x1 = i % map[0].size();
		if (map[y1][x1] != '#')
			continue;

		for (size_t j = i + 1; j < map.size() * map[0].size(); j++)
		{
			const u64 y2 = j / map[0].size();
			const u64 x2 = j % map[0].size();
			if (map[y2][x2] != '#')
				continue;

			result += distance_with_expansion(map, x1, y1, x2, y2, expansion);
		}
	}

	return result;
}

i64 puzzle1(FILE* fp)
{
	return solution(fp, 2);
}

i64 puzzle2(FILE* fp)
{
	return solution(fp, 1'000'000);
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day11_input.txt";

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
