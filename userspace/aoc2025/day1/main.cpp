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
	int amount;
	char direction;

	i32 value = 50;
	i32 result = 0;
	while (fscanf(fp, "%c%d\n", &direction, &amount) == 2)
	{
		switch (direction)
		{
			case 'L':
				value = (value - amount) % 100;
				break;
			case 'R':
				value = (value + amount) % 100;
				break;
		}

		if (value == 0)
			result++;
	}

	return result;
}

i64 part2(FILE* fp)
{
	int amount;
	char direction;

	i32 value = 50;
	i32 result = 0;
	while (fscanf(fp, "%c%d\n", &direction, &amount) == 2)
	{
		switch (direction)
		{
			case 'L':
				if (amount >= value)
					result += (value != 0) + (amount - value) / 100;
				value = (((value - amount) % 100) + 100) % 100;
				break;
			case 'R':
				result += (value + amount) / 100;
				value = (value + amount) % 100;
				break;
		}
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day1_input.txt";

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
