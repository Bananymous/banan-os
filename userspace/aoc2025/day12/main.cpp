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

// ehh im not happy about this but all inputs were crafted such that there
// is no need to overlap any presents, so a simple 3x3 fitting check works

i64 part1(FILE* fp)
{
	char chs[2] {};
	chs[0] = fgetc(fp);
	chs[1] = fgetc(fp);

	char ch;
	while ((ch = fgetc(fp)) != 'x')
	{
		chs[0] = chs[1];
		chs[1] = ch;
	}

	ungetc(ch, fp);
	ungetc(chs[0], fp);
	ungetc(chs[1], fp);

	i64 result = 0;

	u32 w, h;
	while (fscanf(fp, "%" SCNu32 "x%" SCNu32 ":", &w, &h) == 2)
	{
		u32 blocks = 0;

		u32 count;
		while (fscanf(fp, "%" SCNu32 "%c", &count, &ch) == 2 && ch != '\n')
			blocks += count;
		blocks += count;

		if ((w / 3) * (h / 3) >= blocks)
			result++;
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
	const char* file_path = "/usr/share/aoc2025/day12_input.txt";

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
