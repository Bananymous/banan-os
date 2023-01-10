#pragma once

#include <stddef.h>

void kmalloc_initialize();
void kmalloc_dump_nodes();

void* kmalloc_eternal(size_t);
void* kmalloc(size_t);
void kfree(void*);
