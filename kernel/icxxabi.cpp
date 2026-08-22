#include <kernel/Panic.h>

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

extern "C" int __cxa_guard_acquire(uint64_t* g)
{
	uint8_t* guard = reinterpret_cast<uint8_t*>(g);

	for (;;)
	{
		if (BAN::atomic_load(guard[0], BAN::memory_order_acquire))
			return 0;

		uint8_t expected = 0;
		if (BAN::atomic_compare_exchange(guard[1], expected, 1, BAN::memory_order_acquire))
		{
			if (BAN::atomic_load(guard[0], BAN::memory_order_acquire))
			{
				BAN::atomic_store(guard[1], 0, BAN::memory_order_release);
				return 0;
			}
			return 1;
		}

		while (BAN::atomic_load(guard[1], BAN::memory_order_acquire))
			Kernel::Processor::pause();
	}
}

extern "C" void __cxa_guard_release(uint64_t* g)
{
	uint8_t* guard = reinterpret_cast<uint8_t*>(g);
	BAN::atomic_store(guard[0], 1, BAN::memory_order_release);
	BAN::atomic_store(guard[1], 0, BAN::memory_order_release);
}

extern "C" void __cxa_guard_abort(uint64_t*)
{
	Kernel::panic("__cxa_guard_abort");
}
