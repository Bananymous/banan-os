#include <BAN/Math.h>

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

i64 get_max_joltage(const char* line, size_t batteries)
{
	size_t line_len = 0;
	while (isdigit(line[line_len]))
		line_len++;
	if (line_len < batteries)
		return 0;

	i64 result = 0;

	size_t max_i = -1;
	for (size_t battery = 0; battery < batteries; battery++)
	{
		i32 value = 0;
		for (size_t i = max_i + 1; i < line_len - batteries + battery + 1; i++)
		{
			if (line[i] - '0' <= value)
				continue;
			value = line[i] - '0';
			max_i = i;

			if (value == 9)
				break;
		}
		result = (result * 10) + value;
	}

	return result;
}

i64 part1(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
		result += get_max_joltage(buffer, 2);

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
		result += get_max_joltage(buffer, 12);

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day3_input.txt";

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
