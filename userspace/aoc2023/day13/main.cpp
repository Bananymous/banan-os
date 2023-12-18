#include <BAN/Optional.h>
#include <BAN/String.h>
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

using Grid = BAN::Vector<BAN::String>;

BAN::Optional<i64> calculate_score(const Grid& grid, i64 skip_val = 0)
{
	// horizontal reflection
	for (size_t x = 1; x < grid.front().size(); x++)
	{
		const size_t reflection_len = BAN::Math::min(x, grid.front().size() - x);
		bool has_reflection = true;
		for (size_t y = 0; y < grid.size() && has_reflection; y++)
			for (size_t x_off = 0; x_off < reflection_len && has_reflection; x_off++)
				if (grid[y][x - x_off - 1] != grid[y][x + x_off])
					has_reflection = false;
		if (has_reflection && (i64)x != skip_val)
			return x;
	}

	// vertical reflection
	for (size_t y = 1; y < grid.size(); y++)
	{
		const size_t reflection_len = BAN::Math::min(y, grid.size() - y);
		bool has_reflection = true;
		for (size_t x = 0; x < grid.front().size() && has_reflection; x++)
			for (size_t y_off = 0; y_off < reflection_len && has_reflection; y_off++)
				if (grid[y - y_off - 1][x] != grid[y + y_off][x])
					has_reflection = false;
		if (has_reflection && (i64)y * 100 != skip_val)
			return y * 100;
	}

	return {};
}

i64 puzzle1(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		Grid grid;
		while (buffer[0] != '\n')
		{
			ASSERT(buffer[strlen(buffer) - 1] == '\n');
			buffer[strlen(buffer) - 1] = '\0';

			MUST(grid.emplace_back(buffer));

			if (fgets(buffer, sizeof(buffer), fp) == nullptr)
				break;
		}

		result += *calculate_score(grid);
	}

	return result;
}

i64 puzzle2(FILE* fp)
{
	i64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		Grid grid;
		while (buffer[0] != '\n')
		{
			ASSERT(buffer[strlen(buffer) - 1] == '\n');
			buffer[strlen(buffer) - 1] = '\0';

			MUST(grid.emplace_back(buffer));

			if (fgets(buffer, sizeof(buffer), fp) == nullptr)
				break;
		}

		i64 original = *calculate_score(grid);

		for (size_t y = 0; y < grid.size(); y++)
		{
			for (size_t x = 0; x < grid.front().size(); x++)
			{
				grid[y][x] = (grid[y][x] == '.') ? '#' : '.';
				if (auto res = calculate_score(grid, original); res.has_value())
				{
					result += *res;
					goto grid_done;
				}
				grid[y][x] = (grid[y][x] == '.') ? '#' : '.';
			}
		}

		ASSERT_NOT_REACHED();

	grid_done:
		continue;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day13_input.txt";

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
