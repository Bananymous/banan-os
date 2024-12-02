#include <BAN/StringView.h>
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

bool is_safe(i64 curr, i64 prev, bool inc)
{
	i64 abs_diff = BAN::Math::abs(curr - prev);
	if (abs_diff == 0 || abs_diff > 3)
		return false;
	if ((curr > prev) != inc)
		return false;
	return true;
}

i64 part1(FILE* fp)
{
	i64 ret = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		ASSERT(strs.size() >= 2);

		i64 prev = atoll(strs[0].data());
		i64 curr = atoll(strs[1].data());

		const bool inc = (curr > prev);
		if (!is_safe(curr, prev, inc))
			continue;

		bool safe = true;
		for (size_t i = 2; i < strs.size() && safe; i++)
		{
			prev = curr;
			curr = atoll(strs[i].data());
			safe = is_safe(curr, prev, inc);
		}

		ret += safe;
	}

	return ret;
}

i64 part2(FILE* fp)
{
	i64 ret = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		ASSERT(strs.size() >= 3);

		for (size_t skip_idx = 0; skip_idx < strs.size(); skip_idx++)
		{
			i64 prev = atoll(strs[0 + (skip_idx <= 0)].data());
			i64 curr = atoll(strs[1 + (skip_idx <= 1)].data());

			const bool inc = (curr > prev);
			if (!is_safe(curr, prev, inc))
				continue;

			bool safe = true;
			for (size_t i = 2 + (skip_idx <= 2); i < strs.size() && safe; i++)
			{
				if (i == skip_idx)
					continue;
				prev = curr;
				curr = atoll(strs[i].data());
				safe = is_safe(curr, prev, inc);
			}
			if (!safe)
				continue;

			ret++;
			break;
		}
	}

	return ret;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day2_input.txt";

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
