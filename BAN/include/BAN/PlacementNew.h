#pragma once

#include <stddef.h>

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
