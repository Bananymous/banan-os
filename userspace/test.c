#include <stdio.h>
#include <stdlib.h>

#define N 1024

int main()
{
	for (int i = 0; i <= 10; i++)
	{
		int** ptrs = malloc(N * sizeof(int*));
		if (ptrs == NULL)
		{
			perror("malloc");
			return 1;
		}
		for (int j = 0; j < N; j++)
		{
			ptrs[j] = malloc(sizeof(int));
			if (ptrs[j] == NULL)
			{
				perror("malloc");
				return 1;
			}
			*ptrs[j] = j;
			putchar('0' + *ptrs[j] % 10);
		}
		putchar('\n');

		for (int j = 0; j < N; j++)
			free(ptrs[j]);
		free(ptrs);
	}
	
	return 0;
}
