#include <BAN/HashMap.h>
#include <BAN/String.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

using i32 = int32_t;
using i64 = int64_t;

using u32 = uint32_t;
using u64 = uint64_t;

static u32 coord_to_u32(BAN::StringView coord)
{
	ASSERT(coord.size() == 3);
	return ((u32)coord[2] << 16) | ((u32)coord[1] << 8) | (u32)coord[0];
}

i64 puzzle1(FILE* fp)
{
	BAN::HashMap<u32, u32[2]> targets;
	BAN::String left_right_path;

	{
		char buffer[512];
		if (!fgets(buffer, sizeof(buffer), fp))
			return -1;
		MUST(left_right_path.append(buffer));
		left_right_path.pop_back();

		if (!fgets(buffer, sizeof(buffer), fp))
			return -1;
		while (fgets(buffer, sizeof(buffer), fp))
		{
			BAN::StringView line(buffer);
			if (line.size() < 15)
				continue;
			MUST(targets.emplace(
				coord_to_u32(line.substring(0, 3)),
				coord_to_u32(line.substring(7, 3)),
				coord_to_u32(line.substring(12, 3))
			));
		}
	}

	u32 current = coord_to_u32("AAA"sv);
	u32 target = coord_to_u32("ZZZ"sv);

	i64 steps = 0;
	for (; current != target; steps++)
	{
		char direction = left_right_path[steps % left_right_path.size()];
		current = targets[current][direction == 'R'];
	}

	return steps;
}

i64 puzzle2(FILE* fp)
{
	BAN::HashMap<u32, u32[2]> targets;
	BAN::String left_right_path;

	{
		char buffer[512];
		if (!fgets(buffer, sizeof(buffer), fp))
			return -1;
		MUST(left_right_path.append(buffer));
		left_right_path.pop_back();

		if (!fgets(buffer, sizeof(buffer), fp))
			return -1;
		while (fgets(buffer, sizeof(buffer), fp))
		{
			BAN::StringView line(buffer);
			if (line.size() < 15)
				continue;
			MUST(targets.emplace(
				coord_to_u32(line.substring(0, 3)),
				coord_to_u32(line.substring(7, 3)),
				coord_to_u32(line.substring(12, 3))
			));
		}
	}

	i64 result = 1;
	for (auto [key, _] : targets)
	{
		if ((key >> 16) != 'A')
			continue;

		i64 steps = 0;
		for (u32 pos = key; (pos >> 16) != 'Z'; steps++)
		{
			char direction = left_right_path[steps % left_right_path.size()];
			pos = targets[pos][direction == 'R'];
		}

		// Calculating LCM of all steps only works if steps is a multiple of full path size.
		// This seems to be the case with my input, but it's good to verify...
		ASSERT(steps % left_right_path.size() == 0);

		result = BAN::Math::lcm(result, steps);
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day8_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %lld\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %lld\n", puzzle2(fp));

	fclose(fp);
}
