#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

using i32 = int32_t;
using i64 = int64_t;

using u32 = uint32_t;
using u64 = uint64_t;

i64 parse_i64(BAN::StringView sv)
{
	bool negative = (!sv.empty() && sv.front() == '-');
	if (negative)
		sv = sv.substring(1);
	
	i64 result = 0;
	for (char c : sv)
	{
		if (!isdigit(c))
			break;
		result = (result * 10) + (c - '0');
	}
	return negative ? -result : result;
}

i64 puzzle1(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);

		auto value_strs = MUST(line.split(' '));
		if (value_strs.empty())
			continue;

		BAN::Vector<BAN::Vector<i64>> values;
		MUST(values.emplace_back(value_strs.size() + 1, 0));
		for (size_t i = 0; i < value_strs.size(); i++)
			values.back()[i] = parse_i64(value_strs[i]);

		bool all_zeroes = false;
		while (!all_zeroes)
		{
			size_t last = values.size();
			MUST(values.emplace_back(values.back().size() - 1, 0));

			all_zeroes = true;
			for (size_t i = 0; i < values.back().size() - 1; i++)
			{
				i64 lhs = values[last - 1][i + 0];
				i64 rhs = values[last - 1][i + 1];
				values[last][i] = rhs - lhs;
				if (values[last][i] != 0)
					all_zeroes = false;
			}
		}

		for (size_t i = values.size() - 1; i > 0; i--)
		{
			const size_t len = values[i].size();
			i64 lhs = values[i - 1][len - 1];
			i64 res = values[i][len - 1];
			values[i - 1][len - 0] = res + lhs;
		}

		result += values.front().back();
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

		auto value_strs = MUST(line.split(' '));
		if (value_strs.empty())
			continue;

		BAN::Vector<BAN::Vector<i64>> values;
		MUST(values.emplace_back(value_strs.size() + 1, 0));
		for (size_t i = 0; i < value_strs.size(); i++)
			values.back()[i + 1] = parse_i64(value_strs[i]);

		bool all_zeroes = false;
		while (!all_zeroes)
		{
			size_t last = values.size();
			MUST(values.emplace_back(values.back().size() - 1, 0));

			all_zeroes = true;
			for (size_t i = 1; i < values.back().size(); i++)
			{
				i64 lhs = values[last - 1][i + 0];
				i64 rhs = values[last - 1][i + 1];
				values[last][i] = rhs - lhs;
				if (values[last][i] != 0)
					all_zeroes = false;
			}
		}

		for (size_t i = values.size() - 1; i > 0; i--)
		{
			i64 rhs = values[i - 1][1];
			i64 res = values[i][0];
			values[i - 1][0] = rhs - res;
		}

		result += values.front().front();
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day9_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %lld\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %lld\n", puzzle2(fp));

	fclose(fp);
}
