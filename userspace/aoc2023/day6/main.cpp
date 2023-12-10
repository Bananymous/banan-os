#include <BAN/Vector.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>

using i64 = int64_t;

i64 parse_i64_all_digits(BAN::StringView part)
{
	i64 result = 0;
	for (char c : part)
		if (isdigit(c))
			result = (result * 10) + (c - '0');
	return result;
}

i64 puzzle1(FILE* fp)
{
	BAN::Vector<i64> times, distances;

	{
		char buffer[128];
		fgets(buffer, sizeof(buffer), fp);
		BAN::StringView line(buffer);
		line = line.substring(10);
		line = line.substring(0, line.size() - 1);

		auto split = MUST(line.split(' '));
		for (auto part : split)
			MUST(times.push_back(parse_i64_all_digits(part)));
	}

	{
		char buffer[128];
		fgets(buffer, sizeof(buffer), fp);
		BAN::StringView line(buffer);
		line = line.substring(10);
		line = line.substring(0, line.size() - 1);

		auto split = MUST(line.split(' '));
		for (auto part : split)
			MUST(distances.push_back(parse_i64_all_digits(part)));
	}

	ASSERT(times.size() == distances.size());

	i64 result = 1;

	for (size_t i = 0; i < times.size(); i++)
	{
		i64 time = times[i];
		i64 distance = distances[i];

		i64 min = distance / time - 1;
		while ((time - min) * min <= distance)
			min++;

		i64 max = min;
		while ((time - max) * max > distance)
			max++;

		result *= max - min;
	}

	return result;
}

i64 puzzle2(FILE* fp)
{
	i64 time = 0;
	i64 distance = 0;

	{
		char buffer[128];
		fgets(buffer, sizeof(buffer), fp);
		BAN::StringView line(buffer);
		line = line.substring(10);
		time = parse_i64_all_digits(line);
	}

	{
		char buffer[128];
		fgets(buffer, sizeof(buffer), fp);
		BAN::StringView line(buffer);
		line = line.substring(10);
		distance = parse_i64_all_digits(line);
	}

	i64 min = distance / time - 1;
	while ((time - min) * min <= distance)
		min++;

	i64 max = min;
	while ((time - max) * max > distance)
		max++;

	return max - min;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day6_input.txt";

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

	printf("puzzle1: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
