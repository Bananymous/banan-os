#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

int main()
{
	const uid_t euid = geteuid();

	const auto* pw = getpwuid(euid);
	if (pw == nullptr)
	{
		fprintf(stderr, "unknown user id %d\n", euid);
		return 1;
	}

	printf("%s\n", pw->pw_name);

	return 0;
}
