#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <BAN/Swap.h>
#include <BAN/Vector.h>

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

u64 parse_u64(BAN::StringView str)
{
	u64 result = 0;
	for (char c : str)
		if (isdigit(c))
			result = (result * 10) + (c - '0');
	return result;
}

static BAN::HashMap<BAN::String, i64> s_cache;

BAN::String build_cache_key(BAN::StringView record, BAN::Span<u64> groups)
{
	BAN::String cache_key;
	MUST(cache_key.append(record));
	for (u64 group : groups)
		MUST(cache_key.append(BAN::String::formatted(" {}", group)));
	return cache_key;
}

i64 count_possibilities(BAN::StringView record, BAN::Span<u64> groups)
{
	if (groups.empty())
		return !record.contains('#');
	if (record.empty())
		return 0;

	auto cache_key = build_cache_key(record, groups);
	if (s_cache.contains(cache_key))
		return s_cache[cache_key];

	i64 result = 0;

	for (size_t i = 0; i + groups[0] <= record.size(); i++)
	{
		bool valid = true;

		if (record.substring(i, groups[0]).contains('.'))
			valid = false;

		if (i + groups[0] < record.size() && record[i + groups[0]] == '#')
			valid = false;

		if (valid)
		{
			auto next_record = record.substring(i + groups[0]);
			if (!next_record.empty())
				next_record = next_record.substring(1);
			result += count_possibilities(next_record, groups.slice(1));
		}

		if (record[i] == '#')
			break;
	}

	//printf("inserting... "); fflush(stdout);
	MUST(s_cache.insert(cache_key, result));
	//printf("done\n");

	return result;
}

i64 puzzle1(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		if (line.back() == '\n')
			line = line.substring(0, line.size() - 1);
		if (line.size() < 3)
			continue;

		auto parts = MUST(line.split(' '));

		auto record = parts[0];

		auto continuous_strs = MUST(parts[1].split(','));
		BAN::Vector<u64> continuous(continuous_strs.size());
		for (size_t i = 0; i < continuous.size(); i++)
			continuous[i] = parse_u64(continuous_strs[i]);

		result += count_possibilities(record, continuous.span());
	}

	return result;
}

i64 puzzle2(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		if (line.back() == '\n')
			line = line.substring(0, line.size() - 1);
		if (line.size() < 3)
			continue;

		auto parts = MUST(line.split(' '));

		BAN::String record;
		MUST(record.reserve(parts[0].size() + 4));
		for (size_t i = 0; i < 5; i++)
		{
			if (i > 0)
				MUST(record.push_back('?'));
			MUST(record.append(parts[0]));
		}

		auto continuous_strs = MUST(parts[1].split(','));
		BAN::Vector<u64> continuous(5 * continuous_strs.size());
		for (size_t i = 0; i < continuous.size(); i++)
			continuous[i] = parse_u64(continuous_strs[i % continuous_strs.size()]);

		result += count_possibilities(record, continuous.span());
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day12_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %" PRId64 "\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
