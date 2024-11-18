#pragma once

#if __has_include(<new>)
	#include <new>
#else
	#include <stddef.h>

	inline void* operator new(size_t, void* addr) { return addr; }
	inline void* operator new[](size_t, void* addr) { return addr; }
#endif
