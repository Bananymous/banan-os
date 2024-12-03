#include <utime.h>
#include <sys/stat.h>
#include <stdio.h>

int utime(const char* filename, const struct utimbuf* times)
{
	fprintf(stddbg, "TODO: utime(\"%s\", %p)\n", filename, times);

	struct stat st;
	return stat(filename, &st);
}
