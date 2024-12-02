#include <libgen.h>
#include <limits.h>
#include <string.h>

char* basename(char* path)
{
	static char buffer[PATH_MAX];

	constexpr auto prep_string =
		[](const char* str) -> char*
		{
			strcpy(buffer, str);
			return buffer;
		};

	if (path == nullptr || path[0] == '\0')
		return prep_string(".");

	char* endp = path + strlen(path);

	while (endp > path && endp[-1] == '/')
		endp--;
	if (endp == path)
		return prep_string("/");

	char* startp = endp;
	while (startp > path && startp[-1] != '/')
		startp--;

	memcpy(buffer, startp, endp - startp);
	buffer[endp - startp] = '\0';
	return buffer;
}

char* dirname(char* path)
{
	static char buffer[PATH_MAX];

	constexpr auto prep_string =
		[](const char* str) -> char*
		{
			strcpy(buffer, str);
			return buffer;
		};

	if (path == nullptr || path[0] == '\0')
		return prep_string(".");

	char* endp = path + strlen(path);

	while (endp > path && endp[-1] == '/')
		endp--;
	if (endp == path)
		return prep_string("/");

	while (endp > path && endp[-1] != '/')
		endp--;
	if (endp == path)
		return prep_string(".");

	while (endp > path && endp[-1] == '/')
		endp--;
	if (endp == path)
		return prep_string("/");

	memcpy(buffer, path, endp - path);
	buffer[endp - path] = '\0';
	return buffer;
}
