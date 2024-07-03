#include <stdio.h>
#include <unistd.h>

int main()
{
	char* login = getlogin();
	if (login == nullptr)
	{
		printf("unknown user %d\n", geteuid());
		return 1;
	}
	printf("%s\n", login);
	return 0;
}
