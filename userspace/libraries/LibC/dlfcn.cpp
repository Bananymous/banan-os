#include <BAN/Assert.h>

#include <dlfcn.h>

int dlclose(void*)
{
	ASSERT_NOT_REACHED();
}

char* dlerror(void)
{
	ASSERT_NOT_REACHED();
}

void* dlopen(const char*, int)
{
	ASSERT_NOT_REACHED();
}

void* dlsym(void* __restrict, const char* __restrict)
{
	ASSERT_NOT_REACHED();
}
