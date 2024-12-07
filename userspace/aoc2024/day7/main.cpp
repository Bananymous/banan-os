#include <BAN/StringView.h>
#include <BAN/String.h>
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

bool check_if_valid(const BAN::Vector<u64>& vals, size_t idx, u64 curr, bool concat)
{
	// NOTE: This does not apply in general case, but puzzle input
	//       does not contain zeros or negative numbers
	if (curr > vals[0])
		return false;

	if (idx >= vals.size())
		return curr == vals[0];

	if (check_if_valid(vals, idx + 1, curr + vals[idx], concat))
		return true;
	if (check_if_valid(vals, idx + 1, curr * vals[idx], concat))
		return true;

	if (!concat)
		return false;
	u64 mult = 1;
	while (mult <= vals[idx])
		mult *= 10;
	return check_if_valid(vals, idx + 1, curr * mult + vals[idx], concat);
}

i64 part1(FILE* fp)
{
	i64 result = 0;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		if (strs.empty())
			break;

		BAN::Vector<u64> vals;
		for (auto str : strs)
			MUST(vals.push_back(atoll(str.data())));

		if (check_if_valid(vals, 2, vals[1], false))
			result += vals[0];
	}

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		if (strs.empty())
			break;

		BAN::Vector<u64> vals;
		for (auto str : strs)
			MUST(vals.push_back(atoll(str.data())));

		if (check_if_valid(vals, 2, vals[1], true))
			result += vals[0];
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day7_input.txt";

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
