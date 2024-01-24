#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX(a, b) ((a) < (b)) ? (b) : (a)

int parse_int_and_advance(const char*& ptr)
{
	int result = 0;
	while (isdigit(*ptr))
		result = (result * 10) + (*ptr++ - '0');
	return result;
}

int puzzle1(FILE* fp)
{
	int result = 0;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const char* ptr = buffer;

		if (strncmp("Game ", ptr, 5))
			continue;
		ptr += 5;

		int id = parse_int_and_advance(ptr);
		ptr += 2;

		bool valid = true;
		while (*ptr && valid)
		{
			int count = parse_int_and_advance(ptr);

			ptr++;
			if (strncmp(ptr, "red", 3) == 0)
				valid = (count <= 12);
			if (strncmp(ptr, "green", 5) == 0)
				valid = (count <= 13);
			if (strncmp(ptr, "blue", 4) == 0)
				valid = (count <= 14);

			while (*ptr && !isdigit(*ptr))
				ptr++;
		}

		if (valid)
			result += id;
	}

	return result;
}

int puzzle2(FILE* fp)
{	int result = 0;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		const char* ptr = buffer;

		if (strncmp("Game ", ptr, 5))
			continue;
		ptr += 5;

		parse_int_and_advance(ptr);
		ptr += 2;

		int needed_red = 0;
		int needed_green = 0;
		int needed_blue = 0;

		while (*ptr)
		{
			int count = parse_int_and_advance(ptr);

			ptr++;
			if (strncmp(ptr, "red", 3) == 0)
				needed_red = MAX(needed_red, count);
			if (strncmp(ptr, "green", 5) == 0)
				needed_green = MAX(needed_green, count);
			if (strncmp(ptr, "blue", 4) == 0)
				needed_blue = MAX(needed_blue, count);

			while (*ptr && !isdigit(*ptr))
				ptr++;
		}

		result += needed_red * needed_green * needed_blue;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day2_input.txt";
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
