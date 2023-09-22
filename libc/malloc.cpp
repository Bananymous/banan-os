#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static consteval size_t log_size_t(size_t value, size_t base)
{
	size_t result = 0;
	while (value /= base)
		result++;
	return result;
}

static constexpr size_t s_malloc_pool_size_initial = 4096;
static constexpr size_t s_malloc_pool_size_multiplier = 2;
static constexpr size_t s_malloc_pool_count = sizeof(size_t) * 8 - log_size_t(s_malloc_pool_size_initial, s_malloc_pool_size_multiplier);
static constexpr size_t s_malloc_default_align = 16;

struct malloc_node_t
{
	bool allocated;
	size_t size;
	uint8_t data[0];

	size_t data_size() const { return size - sizeof(malloc_node_t); }
	malloc_node_t* next() { return (malloc_node_t*)(data + data_size()); }
};

struct malloc_pool_t
{
	uint8_t* start;
	size_t size;
};

static malloc_pool_t s_malloc_pools[s_malloc_pool_count];

void init_malloc()
{
	size_t pool_size = s_malloc_pool_size_initial;
	for (size_t i = 0; i < s_malloc_pool_count; i++)
	{
		s_malloc_pools[i].start = nullptr;
		s_malloc_pools[i].size = pool_size;
		pool_size *= s_malloc_pool_size_multiplier;
	}
}

static bool allocate_pool(size_t pool_index)
{
	auto& pool = s_malloc_pools[pool_index];
	assert(pool.start == nullptr);

	// allocate memory for pool
	pool.start = (uint8_t*)mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (pool.start == nullptr)
		return false;

	// initialize pool to single unallocated node
	auto* node = (malloc_node_t*)pool.start;
	node->allocated = false;
	node->size = pool.size;

	return true;
}

static void* allocate_from_pool(size_t pool_index, size_t size)
{
	assert(size % s_malloc_default_align == 0);

	auto& pool = s_malloc_pools[pool_index];
	assert(pool.start != nullptr);

	uint8_t* pool_end = pool.start + pool.size;

	for (auto* node = (malloc_node_t*)pool.start; (uint8_t*)node < pool_end; node = node->next())
	{
		if (node->allocated)
			continue;

		{
			// merge two unallocated nodes next to each other
			auto* next = node->next();
			if ((uint8_t*)next < pool_end && !next->allocated)
				node->size += next->size;
		}
		
		if (node->data_size() < size)
			continue;

		node->allocated = true;

		// shrink node if needed
		if (node->data_size() - size > sizeof(malloc_node_t))
		{
			uint8_t* node_end = (uint8_t*)node->next();

			node->size = sizeof(malloc_node_t) + size;

			auto* next = node->next();
			next->allocated = false;
			next->size = node_end - (uint8_t*)next;
		}

		return node->data;
	}

	return nullptr;
}

static malloc_node_t* node_from_data_pointer(void* data_pointer)
{
	return (malloc_node_t*)((uint8_t*)data_pointer - sizeof(malloc_node_t));
}

void* malloc(size_t size)
{
	// align size to s_malloc_default_align boundary
	if (size_t ret = size % s_malloc_default_align)
		size += s_malloc_default_align - ret;

	// find the first pool with size atleast size
	size_t first_usable_pool = 0;
	while (s_malloc_pools[first_usable_pool].size < size)
		first_usable_pool++;
	// first_usable_pool = ceil(log(size/s_malloc_smallest_pool, s_malloc_pool_size_mult))

	// try to find any already existing pools that we can allocate in
	for (size_t i = first_usable_pool; i < s_malloc_pool_count; i++)
		if (s_malloc_pools[i].start != nullptr)
			if (void* ret = allocate_from_pool(i, size))
				return ret;

	// allocate new pool
	for (size_t i = first_usable_pool; i < s_malloc_pool_count; i++)
	{
		if (s_malloc_pools[i].start != nullptr)
			continue;
		if (!allocate_pool(i))
			break;
		return allocate_from_pool(i, size);
	}

	errno = ENOMEM;
	return nullptr;
}

void* realloc(void* ptr, size_t size)
{
	if (ptr == nullptr)
		return malloc(size);
	
	// align size to s_malloc_default_align boundary
	if (size_t ret = size % s_malloc_default_align)
		size += s_malloc_default_align - ret;

	auto* node = node_from_data_pointer(ptr);
	size_t oldsize = node->data_size();

	if (oldsize == size)
		return ptr;

	// shrink allocation if needed
	if (oldsize > size)
	{
		if (node->data_size() - size > sizeof(malloc_node_t))
		{
			uint8_t* node_end = (uint8_t*)node->next();

			node->size = sizeof(malloc_node_t) + size;

			auto* next = node->next();
			next->allocated = false;
			next->size = node_end - (uint8_t*)next;
		}
		return ptr;
	}

	// FIXME: try to expand allocation

	// allocate new pointer
	void* new_ptr = malloc(size);
	if (new_ptr == nullptr)
		return nullptr;
	
	// move data to the new pointer
	size_t bytes_to_copy = oldsize < size ? oldsize : size;
	memcpy(new_ptr, ptr, bytes_to_copy);
	free(ptr);

	return new_ptr;
}

void free(void* ptr)
{
	if (ptr == nullptr)
		return;

	auto* node = node_from_data_pointer(ptr);

	// mark node as unallocated and try to merge with the next node
	node->allocated = false;
	if (!node->next()->allocated)
		node->size += node->next()->size;
}

void* calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	if (size != 0 && total / size != nmemb)
	{
		errno = ENOMEM;
		return nullptr;
	}
	void* ptr = malloc(total);
	if (ptr == nullptr)
		return nullptr;
	memset(ptr, 0, total);
	return ptr;
}
