#include <dlfcn.h>

extern "C" int __dladdr(const void*, Dl_info_t*) __attribute__((weak));
int dladdr(const void* __restrict addr, Dl_info_t* __restrict dlip)
{
	if (&__dladdr == nullptr) [[unlikely]]
		return 0;
	return __dladdr(addr, dlip);
}

extern "C" int __dlclose(void*) __attribute__((weak));
int dlclose(void* handle)
{
	if (&__dlclose == nullptr) [[unlikely]]
		return -1;
	return __dlclose(handle);
}

extern "C" char* __dlerror() __attribute__((weak));
char* dlerror(void)
{
	if (&__dlerror == nullptr) [[unlikely]]
		return const_cast<char*>("TODO: dlfcn functions with static linking");
	return __dlerror();
}

extern "C" void* __dlopen(const char*, int) __attribute__((weak));
void* dlopen(const char* file, int mode)
{
	if (&__dlopen == nullptr) [[unlikely]]
		return nullptr;
	return __dlopen(file, mode);
}

extern "C" void* __dlsym(void*, const char*) __attribute__((weak));
void* dlsym(void* __restrict handle, const char* __restrict name)
{
	if (&__dlsym == nullptr) [[unlikely]]
		return nullptr;
	return __dlsym(handle, name);
}
