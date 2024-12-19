#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/String.h>
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

struct ParseInputResult
{
	BAN::Vector<BAN::String> available_towels;
	BAN::Vector<BAN::String> target_patterns;
};

static ParseInputResult parse_input(FILE* fp)
{
	char buffer[128];

	BAN::String first_line;
	while (fgets(buffer, sizeof(buffer), fp))
	{
		MUST(first_line.append(buffer));
		if (first_line.back() != '\n')
			continue;
		first_line.pop_back();
		break;
	}

	BAN::Vector<BAN::String> available_towels;
	auto first_line_split = MUST(first_line.sv().split(','));
	for (auto splitted : first_line_split)
	{
		if (splitted.starts_with(" "_sv))
			splitted = splitted.substring(1);
		MUST(available_towels.emplace_back(splitted));
	}

	ASSERT(fgets(buffer, sizeof(buffer), fp) && buffer[0] == '\n');

	BAN::Vector<BAN::String> target_patterns;
	while (fgets(buffer, sizeof(buffer), fp))
	{
		usize len = strlen(buffer);
		if (len == 0 || buffer[0] == '\n')
			break;
		ASSERT(buffer[len - 1] == '\n');
		buffer[len - 1] = '\0';
		MUST(target_patterns.emplace_back(BAN::StringView(buffer, len - 1)));
	}

	return ParseInputResult {
		.available_towels = BAN::move(available_towels),
		.target_patterns = BAN::move(target_patterns),
	};
}

static bool recurse_part1(BAN::StringView target, const BAN::Vector<BAN::String>& available, usize index, BAN::HashSet<BAN::StringView>& not_possible)
{
	if (index >= target.size())
		return true;
	if (not_possible.contains(target.substring(index)))
		return false;

	for (const auto& towel : available)
	{
		if (!target.substring(index).starts_with(towel))
			continue;
		if (recurse_part1(target, available, index + towel.size(), not_possible))
			return true;
	}

	MUST(not_possible.insert(target.substring(index)));

	return false;
}

i64 part1(FILE* fp)
{
	auto [available, targets] = parse_input(fp);

	BAN::HashSet<BAN::StringView> not_possible;

	usize result = 0;
	for (const auto& target : targets)
		result += recurse_part1(target, available, 0, not_possible);
	return result;
}

static u64 recurse_part2(BAN::StringView target, const BAN::Vector<BAN::String>& available, usize index, BAN::HashSet<BAN::StringView>& not_possible, BAN::HashMap<BAN::StringView, u64>& possible)
{
	if (index >= target.size())
		return 1;
	if (not_possible.contains(target.substring(index)))
		return 0;

	auto it = possible.find(target.substring(index));
	if (it != possible.end())
		return it->value;

	usize ret = 0;
	for (const auto& towel : available)
	{
		if (!target.substring(index).starts_with(towel))
			continue;
		ret += recurse_part2(target, available, index + towel.size(), not_possible, possible);
	}

	if (ret == 0)
		MUST(not_possible.insert(target.substring(index)));
	else
		MUST(possible.insert_or_assign(target.substring(index), ret));

	return ret;
}

i64 part2(FILE* fp)
{
	auto [available, targets] = parse_input(fp);

	BAN::HashSet<BAN::StringView> not_possible;
	BAN::HashMap<BAN::StringView, u64> possible;

	u64 result = 0;
	for (const auto& target : targets)
		result += recurse_part2(target, available, 0, not_possible, possible);
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day19_input.txt";

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
