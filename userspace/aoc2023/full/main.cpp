#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	for (int i = 1; i <= 19; i++)
	{
		printf("day %d:\n", i);

		char command[128];
		sprintf(command, "/bin/aoc2023/day%d", i);

		system(command);
	}

	return 0;
}
