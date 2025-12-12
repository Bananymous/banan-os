#include <BAN/Vector.h>
#include <BAN/Sort.h>

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

struct Range
{
	u64 min;
	u64 max;
};

static BAN::Vector<Range> parse_ranges(FILE* fp)
{
	BAN::Vector<Range> ranges;

	u64 min, max;
	ungetc('\n', fp);
	while (fscanf(fp, "\n%" SCNu64 "-%" SCNu64, &min, &max) == 2)
		MUST(ranges.emplace_back(min, max));

	return ranges;
}

i64 part1(FILE* fp)
{
	auto ranges = parse_ranges(fp);

	i64 result = 0;

	u64 value;
	while (fscanf(fp, "%" SCNu64, &value) == 1)
	{
		bool is_fresh = false;
		for (const auto range : ranges)
			if (range.min <= value && value <= range.max)
				is_fresh = true;
		result += is_fresh;
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto ranges = parse_ranges(fp);

	BAN::sort::sort(ranges.begin(), ranges.end(),
		[](auto lhs, auto rhs) -> bool
		{
			return lhs.min < rhs.min;
		}
	);

	for (size_t i = 0; i < ranges.size() - 1; i++)
	{
		auto& lhs = ranges[i];
		auto& rhs = ranges[i + 1];
		if (lhs.min > rhs.max || rhs.min > lhs.max)
			continue;

		lhs.min = BAN::Math::min(lhs.min, rhs.min);
		lhs.max = BAN::Math::max(lhs.max, rhs.max);
		ranges.remove(i + 1);

		i--;
	}

	i64 result = 0;
	for (auto range : ranges)
		result += range.max - range.min + 1;
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day5_input.txt";

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
