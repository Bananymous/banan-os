#include <BAN/Array.h>
#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = ssize_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

i64 part1(FILE* fp)
{
	BAN::Vector<BAN::Array<u8, 5>> locks;
	BAN::Vector<BAN::Array<u8, 5>> keys;

	for (;;)
	{
		char buffer[6*7 + 1];
		if (fread(buffer, 1, sizeof(buffer), fp) < sizeof(buffer) - 1)
			break;

		BAN::Array<u8, 5> obj;
		for (usize j = 1; j <= 5; j++)
			for (usize i = 0; i < 5; i++)
				obj[i] += (buffer[6 * j + i] == '#');

		MUST((buffer[0] == '#' ? locks : keys).push_back(obj));
	}

	u32 result = 0;
	for (auto lock : locks)
	{
		for (auto key : keys)
		{
			bool valid = true;
			for (usize i = 0; i < 5 && valid; i++)
				valid = (key[i] + lock[i] <= 5);
			result += valid;
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day25_input.txt";

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
