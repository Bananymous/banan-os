#include <BAN/Memory.h>

namespace std { enum class align_val_t : size_t {}; }

void* operator new(size_t size)							{ return BAN::allocator(size); }
void* operator new[](size_t size)						{ return BAN::allocator(size); }
void* operator new(size_t size, std::align_val_t al)	{ return BAN::allocator_align(size, (size_t)al); }
void* operator new[](size_t size, std::align_val_t al)	{ return BAN::allocator_align(size, (size_t)al); }

void operator delete(void* addr)			{ BAN::deallocator(addr); }
void operator delete[](void* addr)			{ BAN::deallocator(addr); }
void operator delete(void* addr, size_t)	{ BAN::deallocator(addr); }
void operator delete[](void* addr, size_t)	{ BAN::deallocator(addr); }