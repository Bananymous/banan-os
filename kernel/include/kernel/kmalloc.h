#pragma once

#include <stddef.h>

void kmalloc_initialize();
void kmalloc_dump_nodes();

void* kmalloc_eternal(size_t);
void* kmalloc(size_t);
void kfree(void*);


inline void* operator new(size_t size)		{ return kmalloc(size); }
inline void* operator new[](size_t size)	{ return kmalloc(size); }

inline void operator delete(void* addr)				{ kfree(addr); }
inline void operator delete[](void* addr)			{ kfree(addr); }
inline void operator delete(void* addr, size_t)		{ kfree(addr); }
inline void operator delete[](void* addr, size_t)	{ kfree(addr); }