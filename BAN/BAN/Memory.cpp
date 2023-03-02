#include <BAN/Memory.h>

void* operator new(size_t size)								{ return BAN::allocator(size); }
void* operator new[](size_t size)							{ return BAN::allocator(size); }

void operator delete(void* addr)							{ BAN::deallocator(addr); }
void operator delete[](void* addr)							{ BAN::deallocator(addr); }
void operator delete(void* addr, size_t)					{ BAN::deallocator(addr); }
void operator delete[](void* addr, size_t)					{ BAN::deallocator(addr); }
