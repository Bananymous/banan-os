#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Grid2D
{
	size_t width { 0 };
	size_t height { 0 };
	BAN::Vector<char> data;
};

static Grid2D read_grid2d(FILE* fp)
{
	size_t width { 0 };
	size_t height { 0 };
	BAN::Vector<char> data;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const size_t len = strlen(buffer);
		if (len == 0 || buffer[0] == '\n')
			break;
		if (data.empty())
			width = len - 1;
		height++;

		ASSERT(buffer[width] == '\n');

		if (data.capacity() < height * width)
			MUST(data.reserve(2 * height * width));

		MUST(data.resize(height * width));
		memcpy(&data[(height - 1) * width], buffer, width);
	}

	(void)data.shrink_to_fit();

	return Grid2D {
		.width = width,
		.height = height,
		.data = BAN::move(data),
	};
}

static bool check_match(const Grid2D& puzzle, size_t x, size_t y, i32 step_x, i32 step_y, BAN::StringView target)
{
	if (i32 ex = (i32)x + step_x * (i32)(target.size() - 1); ex < 0 || ex >= (i32)puzzle.width)
		return false;
	if (i32 ey = (i32)y + step_y * (i32)(target.size() - 1); ey < 0 || ey >= (i32)puzzle.height)
		return false;
	for (size_t i = 0; i < target.size(); i++)
		if (target[i] != puzzle.data[(y + i * step_y) * puzzle.width + (x + i * step_x)])
			return false;
	return true;
}

i64 part1(FILE* fp)
{
	i64 result = 0;

	auto puzzle = read_grid2d(fp);
	for (size_t y = 0; y < puzzle.height; y++) {
		for (size_t x = 0; x < puzzle.width; x++) {
			result += check_match(puzzle, x, y, 1,  0, "XMAS"_sv);
			result += check_match(puzzle, x, y, 0,  1, "XMAS"_sv);
			result += check_match(puzzle, x, y, 1,  1, "XMAS"_sv);
			result += check_match(puzzle, x, y, 1, -1, "XMAS"_sv);

			result += check_match(puzzle, x, y, 1,  0, "SAMX"_sv);
			result += check_match(puzzle, x, y, 0,  1, "SAMX"_sv);
			result += check_match(puzzle, x, y, 1,  1, "SAMX"_sv);
			result += check_match(puzzle, x, y, 1, -1, "SAMX"_sv);
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	auto puzzle = read_grid2d(fp);
	for (size_t y = 1; y < puzzle.height - 1; y++) {
		for (size_t x = 1; x < puzzle.width - 1; x++) {
			if (!check_match(puzzle, x - 1, y - 1, 1,  1, "MAS") && !check_match(puzzle, x - 1, y - 1, 1,  1, "SAM"))
				continue;
			if (!check_match(puzzle, x - 1, y + 1, 1, -1, "MAS") && !check_match(puzzle, x - 1, y + 1, 1, -1, "SAM"))
				continue;
			result++;
		}
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day4_input.txt";

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
