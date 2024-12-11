#include <BAN/HashMap.h>
#include <BAN/StringView.h>
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

u64 even_divisor(u64 stone)
{
	u8 digits = 0;
	for (u64 mult = 1; mult <= stone; mult *= 10)
		digits++;

	if (digits % 2)
		return 0;

	u64 divisor = 1;
	for (u8 i = 0; i < digits / 2; i++)
		divisor *= 10;

	return divisor;
}

u64 count_stones(u64 stone, u8 depth)
{
	static BAN::HashMap<u64, u64> cache;

	if (depth == 0)
		return 1;

	const u64 cache_key = (stone << 8) | depth;

	auto it = cache.find(cache_key);
	if (it != cache.end())
		return it->value;

	if (stone == 0)
		return count_stones(1, depth - 1);

	u64 divisor = even_divisor(stone);
	if (divisor == 0)
		return count_stones(stone * 2024, depth - 1);

	u64 result = 0;
	result += count_stones(stone / divisor, depth - 1);
	result += count_stones(stone % divisor, depth - 1);
	MUST(cache.insert(cache_key, result));
	return result;
}

i64 part1(FILE* fp)
{
	BAN::Vector<u64> stones;

	char buffer[128];
	ASSERT (fgets(buffer, sizeof(buffer), fp));
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		for (auto str : strs)
			MUST(stones.push_back(atoll(str.data())));
	}

	u64 result = 0;
	for (size_t i = 0; i < stones.size(); i++)
		result += count_stones(stones[i], 25);

	return result;
}

i64 part2(FILE* fp)
{
	BAN::Vector<u64> stones;

	char buffer[128];
	ASSERT (fgets(buffer, sizeof(buffer), fp));
	{
		auto strs = MUST(BAN::StringView(buffer).split(' '));
		for (auto str : strs)
			MUST(stones.push_back(atoll(str.data())));
	}

	u64 result = 0;
	for (size_t i = 0; i < stones.size(); i++)
		result += count_stones(stones[i], 75);

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day11_input.txt";

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
