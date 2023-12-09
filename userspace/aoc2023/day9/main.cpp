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

BAN::Vector<BAN::Vector<i64>> build_difference_tree(BAN::StringView input)
{
	auto value_strs = MUST(input.split(' '));
	if (value_strs.empty())
		return {};

	BAN::Vector<BAN::Vector<i64>> tree;
	MUST(tree.emplace_back(value_strs.size(), 0));
	for (size_t i = 0; i < value_strs.size(); i++)
		tree.back()[i] = parse_i64(value_strs[i]);

	bool all_zeroes = false;
	while (!all_zeroes)
	{
		size_t last = tree.size();
		MUST(tree.emplace_back(tree.back().size() - 1, 0));

		all_zeroes = true;
		for (size_t i = 0; i < tree.back().size(); i++)
		{
			i64 lhs = tree[last - 1][i + 0];
			i64 rhs = tree[last - 1][i + 1];
			tree[last][i] = rhs - lhs;
			if (tree[last][i] != 0)
				all_zeroes = false;
		}
	}

	return tree;
}

i64 puzzle1(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto tree = build_difference_tree(buffer);
		if (tree.empty())
			continue;

		i64 current = 0;
		for (size_t i = tree.size(); i > 0; i--)
			current = tree[i - 1].back() + current;

		result += current;
	}

	return result;
}

i64 puzzle2(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto tree = build_difference_tree(buffer);
		if (tree.empty())
			continue;

		i64 current = 0;
		for (size_t i = tree.size(); i > 0; i--)
			current = tree[i - 1].front() - current;

		result += current;
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
