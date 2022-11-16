#pragma once

#include <stddef.h>

void kmalloc_initialize();

void* kmalloc(size_t);
void kfree(void*);


inline void* operator new(size_t size)   { return kmalloc(size); }
inline void* operator new[](size_t size) { return kmalloc(size); }
