#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct atexit_func_entry_t
{
	void (*func)(void*);
	void* arg;
	void* dso_handle;
};

static atexit_func_entry_t s_atexit_funcs_storage[ATEXIT_MAX];
static atexit_func_entry_t* s_atexit_funcs = s_atexit_funcs_storage;
static size_t s_atexit_funcs_capacity = ATEXIT_MAX;
static size_t s_atexit_func_count = 0;
static pthread_mutex_t s_atexit_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" int __cxa_atexit(void(*func)(void*), void* arg, void* dso_handle)
{
	pthread_mutex_lock(&s_atexit_mutex);

	if (s_atexit_func_count >= s_atexit_funcs_capacity)
	{
		const size_t new_capacity = s_atexit_funcs_capacity * 2;

		void* new_funcs = nullptr;

		if (s_atexit_funcs == s_atexit_funcs_storage)
		{
			new_funcs = malloc(new_capacity * sizeof(atexit_func_entry_t));
			if (new_funcs == nullptr)
				goto __cxa_atexit_error;
			memcpy(new_funcs, s_atexit_funcs, s_atexit_func_count * sizeof(atexit_func_entry_t));
		}
		else
		{
			new_funcs = realloc(s_atexit_funcs, new_capacity * sizeof(atexit_func_entry_t));
			if (new_funcs == nullptr)
				goto __cxa_atexit_error;
		}

		s_atexit_funcs = reinterpret_cast<atexit_func_entry_t*>(new_funcs);
		s_atexit_funcs_capacity = new_capacity;
	}

	s_atexit_funcs[s_atexit_func_count++] = {
		.func = func,
		.arg = arg,
		.dso_handle = dso_handle,
	};

	pthread_mutex_unlock(&s_atexit_mutex);
	return 0;

__cxa_atexit_error:
	pthread_mutex_unlock(&s_atexit_mutex);
	return -1;
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
