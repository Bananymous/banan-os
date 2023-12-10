#include <BAN/HashMap.h>
#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>

using i64 = int64_t;

i64 parse_i64(BAN::StringView sv)
{
	i64 value = 0;
	for (char c : sv)
		value = (value * 10) + (c - '0');
	return value;
}

i64 puzzle1(FILE* fp)
{
	struct Value
	{
		i64 value;
		i64 depth;
	};
	BAN::Vector<Value> current;

	char buffer[256];
	if (!fgets(buffer, sizeof(buffer), fp))
		return -1;
	
	{
		BAN::StringView line(buffer);
		line = line.substring(0, line.size() - 1);

		auto seeds_str = MUST(line.split(' '));
		for (size_t i = 1; i < seeds_str.size(); i++)
			MUST(current.emplace_back(parse_i64(seeds_str[i]), 0));
	}

	fgets(buffer, sizeof(buffer), fp);
	fgets(buffer, sizeof(buffer), fp);

	i64 index = 1;

	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (*buffer == '\n')
		{
			index++;
			fgets(buffer, sizeof(buffer), fp);
			continue;
		}

		BAN::StringView line(buffer);
		line = line.substring(0, line.size() - 1);

		auto values_str = MUST(line.split(' '));

		i64 dst = parse_i64(values_str[0]);
		i64 src = parse_i64(values_str[1]);
		i64 len = parse_i64(values_str[2]);

		for (Value& value : current)
		{
			if (value.depth < index && src <= value.value && value.value < src + len)
			{
				value.value += dst - src;
				value.depth = index;
			}
		}
	}

	i64 result = INT64_MAX;
	for (const auto& value : current)
		result = BAN::Math::min(value.value, result);
	return result;
}

i64 puzzle2(FILE* fp)
{
	struct ValueRange
	{
		i64 value;
		i64 length;
		i64 depth;
	};
	BAN::Vector<ValueRange> current;

	char buffer[256];
	if (!fgets(buffer, sizeof(buffer), fp))
		return -1;
	
	{
		BAN::StringView line(buffer);
		line = line.substring(0, line.size() - 1);
		auto seeds_str = MUST(line.split(' '));
		for (size_t i = 1; i < seeds_str.size(); i += 2)
			MUST(current.emplace_back(parse_i64(seeds_str[i]), parse_i64(seeds_str[i + 1]), 0));
	}

	fgets(buffer, sizeof(buffer), fp);
	fgets(buffer, sizeof(buffer), fp);

	i64 index = 1;

	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (*buffer == '\n')
		{
			index++;
			fgets(buffer, sizeof(buffer), fp);
			continue;
		}

		BAN::StringView line(buffer);
		line = line.substring(0, line.size() - 1);

		auto values_str = MUST(line.split(' '));

		i64 dst = parse_i64(values_str[0]);
		i64 src = parse_i64(values_str[1]);
		i64 len = parse_i64(values_str[2]);

		for (size_t i = 0; i < current.size(); i++)
		{
			auto& range = current[i];
			if (range.depth >= index)
				continue;

			// remap whole range (src range contains full range)
			if (src <= range.value && range.value + range.length <= src + len)
			{
				range.value += dst - src;
				range.depth = index;
				continue;
			}

			// if current range contains any point from src range, split current range
			i64 first_length = 0;
			if (!first_length && range.value <= src && src < range.value + range.length)
				first_length = src - range.value;
			if (!first_length && range.value <= src + len - 1 && src + len - 1 < range.value + range.length)
				first_length = (src + len) - range.value;
			if (first_length)
			{
				ValueRange next_range;
				next_range.value = range.value + first_length;
				next_range.length = range.length - first_length;
				next_range.depth = range.depth;

				range.length = first_length;

				MUST(current.insert(i + 1, next_range));

				i--;
				continue;
			}

			ASSERT(src + len - 1 < range.value || src > range.value + range.length - 1);
		}
	}

	i64 result = INT64_MAX;
	for (const auto& range : current)
		if (range.length)
			result = BAN::Math::min(range.value, result);
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day5_input.txt";

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
