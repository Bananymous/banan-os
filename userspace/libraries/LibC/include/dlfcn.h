#ifndef _DLFCN_H
#define _DLFCN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/dlfcn.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define RTLD_LAZY	0x0
#define RTLD_NOW	0x1
#define RTLD_GLOBAL	0x0
#define RTLD_LOCAL	0x2

#define _RTLD_LAZY_NOW_MASK     0x1
#define _RTLD_GLOBAL_LOCAL_MASK 0x2

#define RTLD_NEXT    ((void*)-1)
#define RTLD_DEFAULT ((void*) 0)

struct Dl_info
{
	const char* dli_fname; /* Pathname of mapped object file. */
	void*       dli_fbase; /* Base of mapped address range. */
	const char* dli_sname; /* Sumbol name or null pointer. */
	void*       dli_saddr; /* Symbol address or null pointer. */
};

typedef struct Dl_info Dl_info_t; /* POSIX type */
typedef struct Dl_info Dl_info;   /* Linux type */

int		dladdr(const void* __restrict addr, Dl_info_t* __restrict dlip);
int		dlclose(void* handle);
char*	dlerror(void);
void*	dlopen(const char* file, int mode);
void*	dlsym(void* __restrict handle, const char* __restrict name);

__END_DECLS

#endif
