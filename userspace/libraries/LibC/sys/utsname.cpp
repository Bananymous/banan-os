#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#define __xstr(s) __str(s)
#define __str(s) #s

int uname(struct utsname* name)
{
	strcpy(name->sysname, "banan-os");
	if (gethostname(name->nodename, sizeof(name->nodename)) == -1)
		return -1;
	strcpy(name->release, "0.0.0-banan_os");
	strcpy(name->version, __DATE__ " " __TIME__);
	strcpy(name->machine, __xstr(__arch));
	return 0;
}
