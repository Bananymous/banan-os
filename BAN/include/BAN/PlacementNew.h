#pragma once

#include <stddef.h>

#ifdef __banan_os__
inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
#else
#include <new>
#endif
