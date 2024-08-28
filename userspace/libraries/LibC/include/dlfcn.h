#ifndef _DLFCN_H
#define _DLFCN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/dlfcn.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define RTLD_LAZY	1
#define RTLD_NOW	2
#define RTLD_GLOBAL	3
#define RTLD_LOCAL	4

#define RTLD_NEXT    ((void*)-1)
#define RTLD_DEFAULT ((void*) 0)

int		dlclose(void* handle);
char*	dlerror(void);
void*	dlopen(const char* file, int mode);
void*	dlsym(void* __restrict handle, const char* __restrict name);

__END_DECLS

#endif
