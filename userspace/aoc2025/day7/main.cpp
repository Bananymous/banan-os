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

static BAN::Vector<BAN::String> parse_lines(FILE* fp)
{
	BAN::Vector<BAN::String> lines;
	MUST(lines.emplace_back());

	char line_buffer[128];
	while (fgets(line_buffer, sizeof(line_buffer), fp))
	{
		MUST(lines.back().append(line_buffer));
		if (lines.back().back() != '\n')
			continue;
		lines.back().pop_back();
		MUST(lines.emplace_back());
	}

	while (lines.back().empty())
		lines.pop_back();

	return lines;
}

i64 part1(FILE* fp)
{
	i32 result = 0;

	auto lines = parse_lines(fp);

	for (char& ch : lines.front())
		if (ch == 'S')
			ch = '|';

	for (size_t y = 0; y < lines.size() - 1; y++)
	{
		for (size_t x = 0; x < lines[y].size(); x++)
		{
			if (lines[y][x] != '|')
				continue;

			switch (lines[y + 1][x])
			{
				case '.':
					lines[y + 1][x] = '|';
					break;
				case '^':
					lines[y + 1][x - 1] = '|';
					lines[y + 1][x + 1] = '|';
					result++;
					break;
			}
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto lines = parse_lines(fp);

	const size_t h = lines.size();
	const size_t w = lines.front().size();

	BAN::Vector<i64> timelines;
	MUST(timelines.resize(w * h));

	const auto get_char_value = [](char ch) { return ch == '.' ? 0 : ch == 'S' ? 1 : -1; };
	for (size_t y = 0; y < h; y++)
		for (size_t x = 0; x < w; x++)
			timelines[y * w + x] = get_char_value(lines[y][x]);

	for (size_t y = 1; y < h; y++)
	{
		for (size_t x = 0; x < w; x++)
		{
			if (timelines[(y - 1) * w + x] < 0)
				;
			else if (timelines[y * w + x] >= 0)
				timelines[y * w + x] += timelines[(y - 1) * w + x];
			else
			{
				timelines[y * w + x - 1] += timelines[(y - 1) * w + x];
				timelines[y * w + x + 1] += timelines[(y - 1) * w + x];
			}
		}
	}

	i64 result = 0;
	for (size_t x = 0; x < w; x++)
		if (auto value = timelines[(h - 1) * w + x]; value > 0)
			result += value;
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day7_input.txt";

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
