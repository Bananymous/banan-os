#include <malloc.h>

void* operator new(size_t size)   { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }

void operator delete(void* addr)           { free(addr); }
void operator delete[](void* addr)         { free(addr); }
void operator delete(void* addr, size_t)   { free(addr); }
void operator delete[](void* addr, size_t) { free(addr); }
