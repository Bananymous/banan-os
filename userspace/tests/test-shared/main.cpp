#include <errno.h>
#include <stdio.h>

int main()
{
	printf("printf works\n");
	fprintf(stdout, "fprintf(stdout) works!\n");

	errno = ENOTSUP;
	perror(nullptr);
}
