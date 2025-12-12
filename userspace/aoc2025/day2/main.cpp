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

i64 part1(FILE* fp)
{
	const auto is_invalid_id =
		[](u64 id) -> bool
		{
			u64 temp_id = id;
			u64 repeat = 0;
			u64 mult = 1;

			while (repeat * mult + repeat < id)
			{
				repeat += (temp_id % 10) * mult;
				temp_id /= 10;
				mult *= 10;
			}

			if (temp_id < mult / 10)
				return false;
			return repeat * mult + repeat == id;
		};

	u64 result = 0;

	u64 start, end;
	while (fscanf(fp, "%" SCNu64 "-%" SCNu64 ",", &start, &end) >= 2)
		for (u64 id = start; id <= end; id++)
			if (is_invalid_id(id))
				result += id;

	return result;
}

i64 part2(FILE* fp)
{
	const auto is_invalid_id =
		[](u64 id) -> bool
		{
			u64 temp_id = id;
			u64 repeat = 0;
			u64 mult = 1;

			while (repeat * mult + repeat < id)
			{
				repeat += (temp_id % 10) * mult;
				temp_id /= 10;
				mult *= 10;

				if (repeat < mult / 10)
					continue;

				u64 attempt = repeat * mult + repeat;
				while (attempt < id)
					attempt = attempt * mult + repeat;
				if (attempt == id)
					return true;
			}

			return false;
		};

	u64 result = 0;

	u64 start, end;
	while (fscanf(fp, "%" SCNu64 "-%" SCNu64 ",", &start, &end) >= 2)
		for (u64 id = start; id <= end; id++)
			if (is_invalid_id(id))
				result += id;

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day2_input.txt";

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
