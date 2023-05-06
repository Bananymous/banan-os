#include <stdio.h>
#include <stdlib.h>

int main()
{
	for (int i = 0; i < 10; i++)
	{
		int* ptrs[10];
		for (int j = 0; j < 10; j++)
		{
			ptrs[j] = malloc(10);
			if (ptrs[j] == NULL)
			{
				perror("malloc");
				return 1;
			}
			*ptrs[j] = j;
			putc('0' + *ptrs[j], stdout);
		}
		for (int j = 0; j < 10; j++)
			free(ptrs[j]);
		putc('\n', stdout);
	}
	
	return 0;
}
