#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main()
{
	for (int i = 1; i <= 25; i++)
	{
		char command[128];
		sprintf(command, "/bin/aoc2025/aoc2025_day%d", i);

		struct stat st;
		if (stat(command, &st) == -1)
			continue;

		printf("day%d\n", i);

		system(command);
	}

	return 0;
}
