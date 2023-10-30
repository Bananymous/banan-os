#include <icxxabi.h>

#define ATEXIT_MAX_FUNCS 128

struct atexit_func_entry_t
{
	void (*destructor)(void*);
	void* data;
	void* dso_handle;
};

static atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];
static int __atexit_func_count = 0;

int __cxa_atexit(void (*func)(void*), void* data, void* dso_handle)
{
	if (__atexit_func_count >= ATEXIT_MAX_FUNCS)
		return -1;;
	__atexit_funcs[__atexit_func_count].destructor = func;
	__atexit_funcs[__atexit_func_count].data = data;
	__atexit_funcs[__atexit_func_count].dso_handle = dso_handle;
	__atexit_func_count++;
	return 0;
};

void __cxa_finalize(void* func)
{
	for (int i = __atexit_func_count - 1; i >= 0; i--)
	{
		if (func && func != __atexit_funcs[i].destructor)
			continue;
		if (__atexit_funcs[i].destructor == nullptr)
			continue;
		__atexit_funcs[i].destructor(__atexit_funcs[i].data);
		__atexit_funcs[i].destructor = nullptr;
	}
}
