#include <stddef.h>
#include <limits.h>

struct atexit_func_entry_t
{
	void (*func)(void*);
	void* arg;
	void* dso_handle;
};

static atexit_func_entry_t s_atexit_funcs[ATEXIT_MAX];
static size_t s_atexit_func_count = 0;

extern "C" int __cxa_atexit(void(*func)(void*), void* arg, void* dso_handle)
{
	if (s_atexit_func_count >= ATEXIT_MAX)
		return -1;
	s_atexit_funcs[s_atexit_func_count++] = {
		.func = func,
		.arg = arg,
		.dso_handle = dso_handle,
	};
	return 0;
};

extern "C" void __cxa_finalize(void* dso_handle)
{
	for (size_t i = s_atexit_func_count; i > 0; i--)
	{
		auto& atexit_func = s_atexit_funcs[i - 1];
		if (atexit_func.func == nullptr)
			continue;
		if (dso_handle && dso_handle != atexit_func.dso_handle)
			continue;
		atexit_func.func(atexit_func.arg);
		atexit_func.func = nullptr;
	}
};
