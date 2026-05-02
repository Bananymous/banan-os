#pragma once

#include <stddef.h>

void kmalloc_initialize();
void kmalloc_dump_info();

void* kmalloc(size_t size);
void* kmalloc(size_t size, size_t align);
void kfree(void*);
