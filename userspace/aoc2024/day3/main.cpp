#include <BAN/String.h>

#include <inttypes.h>
#include <stdio.h>
#include <ctype.h>

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
	i64 result = 0;

	BAN::String full_input;

	{
		char buffer[512];
		while (fgets(buffer, sizeof(buffer), fp))
			MUST(full_input.append(buffer));
	}

	result = 0;
	for (size_t i = 0; i < full_input.size() - 4;)
	{
		if (!full_input.sv().substring(i).starts_with("mul("))
		{
			i++;
			continue;
		}

		i += 4;
		if (i >= full_input.size() || !isdigit(full_input[i]))
			continue;

		i64 lhs = atoll(&full_input[i]);
		while (i < full_input.size() && isdigit(full_input[i]))
			i++;

		if (i >= full_input.size() || full_input[i] != ',')
			continue;

		i++;
		if (i >= full_input.size() || !isdigit(full_input[i]))
			continue;

		int rhs = atoll(&full_input[i]);
		while (i < full_input.size() && isdigit(full_input[i]))
			i++;

		if (i >= full_input.size() || full_input[i] != ')')
			continue;

		result += lhs * rhs;
	}

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	BAN::String full_input;

	{
		char buffer[512];
		while (fgets(buffer, sizeof(buffer), fp))
			MUST(full_input.append(buffer));
	}

	bool state = true;

	result = 0;
	for (size_t i = 0; i < full_input.size();)
	{
		if (full_input.sv().substring(i).starts_with("do()"_sv))
		{
			state = true;
			i += 4;
			continue;
		}

		if (full_input.sv().substring(i).starts_with("don't()"_sv))
		{
			state = false;
			i += 6;
			continue;
		}

		if (state == false || !full_input.sv().substring(i).starts_with("mul("))
		{
			i++;
			continue;
		}

		i += 4;
		if (i >= full_input.size() || !isdigit(full_input[i]))
			continue;

		i64 lhs = atoll(&full_input[i]);
		while (i < full_input.size() && isdigit(full_input[i]))
			i++;

		if (i >= full_input.size() || full_input[i] != ',')
			continue;

		i++;
		if (i >= full_input.size() || !isdigit(full_input[i]))
			continue;

		int rhs = atoll(&full_input[i]);
		while (i < full_input.size() && isdigit(full_input[i]))
			i++;

		if (i >= full_input.size() || full_input[i] != ')')
			continue;

		result += lhs * rhs;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day3_input.txt";

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
