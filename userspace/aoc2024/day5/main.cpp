#include <BAN/Array.h>
#include <BAN/Vector.h>
#include <BAN/StringView.h>

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

i64 part1(FILE* fp)
{
	bool requirements[100][100] {};
	char buffer[128];

	int a, b;
	while (fgets(buffer, sizeof(buffer), fp) && sscanf(buffer, "%d|%d", &a, &b) == 2)
		requirements[b][a] = true;

	i64 result = 0;

	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::Vector<int> update;
		for (size_t i = 0; isdigit(buffer[i]); i += 3)
			MUST(update.push_back(atoi(&buffer[i])));

		bool valid = true;
		for (size_t i = 0; i < update.size() && valid; i++)
			for (size_t j = i + 1; j < update.size() && valid; j++)
				if (requirements[update[i]][update[j]])
					valid = false;

		if (valid)
			result += update[update.size() / 2];
	}

	return result;
}

i64 part2(FILE* fp)
{
	bool requirements[100][100] {};
	char buffer[128];

	int a, b;
	while (fgets(buffer, sizeof(buffer), fp) && sscanf(buffer, "%d|%d", &a, &b) == 2)
		requirements[b][a] = true;

	i64 result = 0;

	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::Vector<int> update;
		for (size_t i = 0; isdigit(buffer[i]); i += 3)
			MUST(update.push_back(atoi(&buffer[i])));

		bool correct { true };
		for (size_t i = 0; i < update.size(); i++)
		{
			for (size_t j = i + 1; j < update.size(); j++)
			{
				if (requirements[update[i]][update[j]])
				{
					BAN::swap(update[i], update[j]);
					correct = false;
					i--;
					break;
				}
			}
		}

		if (!correct)
			result += update[update.size() / 2];
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day5_input.txt";

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
