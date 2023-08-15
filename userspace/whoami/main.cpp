#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
	auto* pw = getpwuid(geteuid());
	if (pw == nullptr)
	{
		printf("unknown user %d\n", geteuid());
		return 1;
	}
	printf("%s\n", pw->pw_name);
	endpwent();
	return 0;
}
