#pragma once

#include <BAN/Optional.h>
#include <kernel/Memory/Types.h>

#include <stddef.h>

void kmalloc_initialize();
void kmalloc_dump_info();

void* kmalloc(size_t size);
void* kmalloc(size_t size, size_t align, bool force_identity_map = false);
void kfree(void*);

BAN::Optional<Kernel::paddr_t> kmalloc_paddr_of(Kernel::vaddr_t);
BAN::Optional<Kernel::vaddr_t> kmalloc_vaddr_of(Kernel::paddr_t);
