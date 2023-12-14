#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int puzzle1(FILE* fp)
{
	using BAN::Vector, BAN::String;

	Vector<String> lines;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		buffer[strlen(buffer) - 1] = '\0';
		MUST(lines.emplace_back(buffer));
	}	

	int result = 0;

	for (size_t y = 0; y < lines.size(); y++)
	{
		for (size_t x = 0; x < lines[y].size(); x++)
		{			
			if (!isdigit(lines[y][x]))
				continue;

			bool should_add = false;

			for (ssize_t y_off = -1; y_off <= 1; y_off++)
			{
				if ((ssize_t)y + y_off < 0)
					continue;
				if (y + y_off >= lines.size())
					break;

				for (ssize_t x_off = -1;; x_off++)
				{
					if ((ssize_t)x + x_off < 0)
						continue;
					if (x + x_off >= lines[y + y_off].size())
						break;
					if (x_off > 0 && !isdigit(lines[y][x + x_off - 1]))
						break;

					char c = lines[y + y_off][x + x_off];
					if (!isdigit(c) && c != '.')
					{
						should_add = true;
						break;
					}
				}	

				if (should_add)
					break;
			}

			int number = 0;
			for (; x < lines[y].size(); x++)
			{
				if (!isdigit(lines[y][x]))
				{
					x--;
					break;
				}
				number = (number * 10) + (lines[y][x] - '0');
			}

			if (should_add)
				result += number;
		}
	}

	return result;
}

int puzzle2(FILE* fp)
{
	using BAN::Vector, BAN::String, BAN::HashMap;

	Vector<String> lines;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		buffer[strlen(buffer) - 1] = '\0';
		MUST(lines.emplace_back(buffer));
	}	

	// Didn't want to think about O(1) space, this is much simpler.
	// Map numbers next to '*' to asterisk's coordinates.
	HashMap<uint32_t, Vector<int>> gears;

	for (size_t y = 0; y < lines.size(); y++)
	{
		for (size_t x = 0; x < lines[y].size(); x++)
		{
			if (!isdigit(lines[y][x]))
				continue;

			int number = 0;
			for (int i = 0; x + i < lines[y].size(); i++)
			{
				if (!isdigit(lines[y][x + i]))
					break;
				number = (number * 10) + (lines[y][x + i] - '0');
			}

			for (ssize_t y_off = -1; y_off <= 1; y_off++)
			{
				if ((ssize_t)y + y_off < 0)
					continue;
				if (y + y_off >= lines.size())
					break;

				for (ssize_t x_off = -1;; x_off++)
				{
					if ((ssize_t)x + x_off < 0)
						continue;
					if (x + x_off >= lines[y + y_off].size())
						break;
					if (x_off > 0 && !isdigit(lines[y][x + x_off - 1]))
						break;

					if (lines[y + y_off][x + x_off] == '*')
					{
						uint32_t index = (y + y_off) << 16 | (x + x_off);
						if (!gears.contains(index))
							MUST(gears.insert(index, {}));
						MUST(gears[index].push_back(number));
					}
				}
			}

			for (; x < lines[y].size(); x++)
			{
				if (isdigit(lines[y][x]))
					continue;
				x--;
				break;
			}
		}
	}

	int result = 0;
	for (auto& [_, nums] : gears)
	{
		if (nums.size() != 2)
			continue;
		result += nums[0] * nums[1];
	}
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day3_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %d\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %d\n", puzzle2(fp));

	fclose(fp);
}
