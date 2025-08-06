#include <dirent.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>

static int selector(const dirent* dirent)
{
	if (strcmp(dirent->d_name, "lo") == 0)
		return 1;
	if (strncmp(dirent->d_name, "eth", 3) == 0)
		return 1;
	return 0;
}

static int comparator(const dirent** d1, const dirent** d2)
{
	if (strcmp((*d1)->d_name, "lo"))
		return -1;
	if (strcmp((*d2)->d_name, "lo"))
		return +1;
	return alphasort(d1, d2);
}

char* if_indextoname(unsigned ifindex, char* ifname)
{
	if (ifindex == 0)
		return nullptr;

	dirent** namelist;

	const int count = scandir("/dev", &namelist, selector, comparator);
	if (count == -1)
		return nullptr;

	char* result = nullptr;
	if (ifindex - 1 < static_cast<unsigned>(count))
	{
		strcpy(ifname, namelist[ifindex - 1]->d_name);
		result = ifname;
	}

	for (int i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	return result;
}

unsigned if_nametoindex(const char* ifname)
{
	dirent** namelist;

	const int count = scandir("/dev", &namelist, selector, comparator);
	if (count == -1)
		return 0;

	unsigned result = 0;
	for (int i = 0; i < count; i++)
	{
		if (strcmp(ifname, namelist[i]->d_name) != 0)
			continue;
		result = i + 1;
		break;
	}

	for (int i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	return result;
}
