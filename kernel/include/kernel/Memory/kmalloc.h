#pragma once

#include <stddef.h>

void kmalloc_initialize();

void* kmalloc(size_t);
void kfree(void*);
