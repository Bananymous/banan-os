#include <stdio.h>
#include <unistd.h>

int main()
{
	printf("uid %u, gid %u, euid %u, egid %u\n", getuid(), getgid(), geteuid(), getegid());
}
