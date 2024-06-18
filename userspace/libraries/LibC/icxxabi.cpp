#include <BAN/Assert.h>
#include <stdint.h>
#include <stddef.h>

#define ATEXIT_MAX_FUNCS 128

struct atexit_func_entry_t
{
	void(*func)(void*);
	void* arg;
	void* dso_handle;
};

static atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];
static size_t __atexit_func_count = 0;

extern "C" int __cxa_atexit(void(*func)(void*), void* arg, void* dso_handle)
{
	if (__atexit_func_count >= ATEXIT_MAX_FUNCS)
		return -1;
	auto& atexit_func = __atexit_funcs[__atexit_func_count++];
	atexit_func.func = func;
	atexit_func.arg = arg;
	atexit_func.dso_handle = dso_handle;
	return 0;
};

extern "C" void __cxa_finalize(void* f)
{
	for (size_t i = __atexit_func_count; i > 0; i--)
	{
		auto& atexit_func = __atexit_funcs[i - 1];
		if (atexit_func.func == nullptr)
			continue;
		if (f == nullptr || f == atexit_func.func)
		{
			atexit_func.func(atexit_func.arg);
			atexit_func.func = nullptr;
		}
	}
};
