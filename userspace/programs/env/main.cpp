#include <stdio.h>

extern char** environ;

int main()
{
	if (!environ)
		return 0;
	char** current = environ;
	while (*current)
			printf("%s\n", *current++);
	return 0;
}
