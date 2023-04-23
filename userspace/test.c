#include <stdio.h>

int main()
{
	if (printf("Hello %s!", "World") == -1)
		perror(NULL);
	return 0;
}
