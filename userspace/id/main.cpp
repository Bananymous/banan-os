#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

int main()
{
	uid_t uid = getuid();
	uid_t euid = geteuid();

	gid_t gid = getgid();
	gid_t egid = getegid();

	passwd* pw_uid = getpwuid(uid);
	if (pw_uid == nullptr)
	{
		fprintf(stderr, "Unknown user #%d\n", uid);
		return 1;
	}

	passwd* pw_euid = getpwuid(euid);
	if (pw_euid == nullptr)
	{
		fprintf(stderr, "Unknown user #%d\n", euid);
		return 1;
	}

	printf("uid=%u(%s)", uid, pw_uid->pw_name);
	if (uid != euid)
		printf(",euid=%u(%s)",euid, pw_euid->pw_name);

	printf(" gid=%u", gid);
	if (gid != egid)
		printf(",egid=%u", egid);

	printf("\n");
}
