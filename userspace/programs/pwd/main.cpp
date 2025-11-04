#include <limits.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
	char buffer[PATH_MAX];
	if (getcwd(buffer, PATH_MAX) == nullptr)
	{
		perror("getcwd");
		return 1;
	}

	printf("%s\n", buffer);
}
