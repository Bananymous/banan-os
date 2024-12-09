#include <BAN/Vector.h>

#include <ctype.h>
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

static BAN::Vector<u32> parse_input(FILE* fp)
{
	BAN::Vector<u32> result;

	u32 idx = 0;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		usize len = strlen(buffer);
		if (len > 0 && buffer[len - 1] == '\n')
			len--;

		for (usize i = 0; i < len; i++)
		{
			const u32 block_id = (idx % 2) ? 0xFFFFFFFF : idx / 2;

			ASSERT(isdigit(buffer[i]));
			for (i8 j = 0; j < buffer[i] - '0'; j++)
				MUST(result.push_back(block_id));

			idx++;
		}
	}

	return result;
}

i64 part1(FILE* fp)
{
	auto input = parse_input(fp);

	usize l = 0;
	usize r = input.size() - 1;

	i64 result = 0;

	while (l <= r)
	{
		if (input[l] != 0xFFFFFFFF)
		{
			result += input[l] * l;
			l++;
			continue;
		}

		while (l < r && input[r] == 0xFFFFFFFF)
			r--;

		if (l == r)
			continue;

		result += input[r] * l;
		r--;
		l++;
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto input = parse_input(fp);

	for (usize r = input.size(); r > 0;)
	{
		if (input[r - 1] == 0xFFFFFFFF)
		{
			r--;
			continue;
		}

		const u32 id = input[r - 1];

		usize rlen = 0;
		while (r - rlen - 1 > 0 && input[r - rlen - 1] == id)
			rlen++;

		usize l = 0;
		for (; l < r - rlen; l++)
		{
			if (input[l] != 0xFFFFFFFF)
				continue;

			usize llen = 1;
			while (llen < rlen && input[l + llen] == 0xFFFFFFFF)
				llen++;

			if (llen == rlen)
				break;
		}

		if (l < r - rlen)
		{
			for (usize i = 0; i < rlen; i++)
				input[r - i - 1] = 0xFFFFFFFF;
			for (usize i = 0; i < rlen; i++)
				input[l + i] = id;
		}

		r -= rlen;
		while (r > 0 && input[r - 1] == id)
			r--;
	}

	i64 result = 0;
	for (usize i = 0; i < input.size(); i++)
		if (input[i] != 0xFFFFFFFF)
			result += i * input[i];

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day9_input.txt";

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
