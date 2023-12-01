#include <ctype.h>
#include <stdio.h>
#include <string.h>

int puzzle1(FILE* fp)
{
	int sum = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		int line_len = strlen(buffer);

		for (int i = 0; i < line_len; i++)
		{
			if (!isdigit(buffer[i]))
				continue;
			sum += 10 * (buffer[i] - '0');
			break;
		}

		for (int i = line_len - 1; i >= 0; i--)
		{
			if (!isdigit(buffer[i]))
				continue;
			sum += buffer[i] - '0';
			break;
		}
	}

	return sum;
}

int puzzle2(FILE* fp)
{
	const char* str_digits[] = {
		"zero",
		"one",
		"two",
		"three",
		"four",
		"five",
		"six",
		"seven",
		"eight",
		"nine"
	};

	int sum = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		int line_len = strlen(buffer);

		int digit = -1;

		for (int i = 0; i < line_len; i++)
		{
			if (isdigit(buffer[i]))
			{
				digit = buffer[i] - '0';
				break;
			}
			for (int j = 0; j < 10; j++)
			{
				if (line_len - i < strlen(str_digits[j]))
					continue;
				if (strncmp(buffer + i, str_digits[j], strlen(str_digits[j])) == 0)
				{
					digit = j;
					break;
				}
			}
			if (digit != -1)
				break;
		}
		sum += 10 * digit;

		digit = -1;

		for (int i = line_len - 1; i >= 0; i--)
		{
			if (isdigit(buffer[i]))
			{
				digit = buffer[i] - '0';
				break;
			}
			for (int j = 0; j < 10; j++)
			{
				if (i < strlen(str_digits[j]))
					continue;
				if (strncmp(buffer + i - strlen(str_digits[j]), str_digits[j], strlen(str_digits[j])) == 0)
				{
					digit = j;
					break;
				}
			}
			if (digit != -1)
				break;
		}

		sum += digit;
	}

	return sum;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day1_input.txt";
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
