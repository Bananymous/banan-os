#include <BAN/Debug.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define DEBUG_MALLOC 0

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
static constexpr size_t s_malloc_default_align = alignof(max_align_t);
// This is indirectly smallest allowed allocation
static constexpr size_t s_malloc_shrink_threshold = 64;

struct malloc_node_t
{
	// TODO: these two pointers could be put into data region
	malloc_node_t* prev_free;
	malloc_node_t* next_free;
	size_t size;
	bool allocated;
	bool last;
	alignas(s_malloc_default_align) uint8_t data[0];

	size_t data_size() const { return size - sizeof(malloc_node_t); }
	malloc_node_t* next() { return (malloc_node_t*)(data + data_size()); }
};

struct malloc_pool_t
{
	uint8_t* start;
	size_t size;

	malloc_node_t* free_list;

	uint8_t* end() { return start + size; }
	bool contains(malloc_node_t* node) { return start <= (uint8_t*)node && (uint8_t*)node < end(); }
};

static malloc_pool_t s_malloc_pools[s_malloc_pool_count];

void _init_malloc()
{
	size_t pool_size = s_malloc_pool_size_initial;
	for (size_t i = 0; i < s_malloc_pool_count; i++)
	{
		s_malloc_pools[i].start = nullptr;
		s_malloc_pools[i].size = pool_size;
		s_malloc_pools[i].free_list = nullptr;;
		pool_size *= s_malloc_pool_size_multiplier;
	}
}

static bool allocate_pool(size_t pool_index)
{
	auto& pool = s_malloc_pools[pool_index];
	assert(pool.start == nullptr);

	// allocate memory for pool
	void* new_pool = mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (new_pool == MAP_FAILED)
		return false;

	pool.start = (uint8_t*)new_pool;

	// initialize pool to single unallocated node
	auto* node = (malloc_node_t*)pool.start;
	node->allocated = false;
	node->size = pool.size;
	node->last = true;
	node->prev_free = nullptr;
	node->next_free = nullptr;

	pool.free_list = node;

	return true;
}

static void remove_node_from_pool_free_list(malloc_pool_t& pool, malloc_node_t* node)
{
	if (node == pool.free_list)
	{
		pool.free_list = pool.free_list->next_free;
		if (pool.free_list)
			pool.free_list->prev_free = nullptr;
	}
	else
	{
		if (node->next_free)
			node->next_free->prev_free = node->prev_free;
		if (node->prev_free)
			node->prev_free->next_free = node->next_free;
	}
}

static void* allocate_from_pool(size_t pool_index, size_t size)
{
	assert(size % s_malloc_default_align == 0);

	auto& pool = s_malloc_pools[pool_index];
	assert(pool.start != nullptr);

	if (!pool.free_list)
		return nullptr;

	for (auto* node = pool.free_list; node; node = node->next_free)
	{
		assert(!node->allocated);

		// merge nodes right after current one
		while (!node->last && !node->next()->allocated)
		{
			auto* next = node->next();
			remove_node_from_pool_free_list(pool, next);
			node->last = next->last;
			node->size += next->size;
		}

		if (node->data_size() < size)
			continue;

		node->allocated = true;
		remove_node_from_pool_free_list(pool, node);

		// shrink node if needed
		if (node->data_size() - size >= sizeof(malloc_node_t) + s_malloc_shrink_threshold)
		{
			uint8_t* node_end = (uint8_t*)node->next();

			node->size = sizeof(malloc_node_t) + size;

			auto* next = node->next();
			next->allocated = false;
			next->size = node_end - (uint8_t*)next;
			next->last = node->last;

			node->last = false;

			// insert excess node to free list
			if (pool.free_list)
				pool.free_list->prev_free = next;
			next->next_free = pool.free_list;
			next->prev_free = nullptr;
			pool.free_list = next;
		}

		return node->data;
	}

	return nullptr;
}

static malloc_node_t* node_from_data_pointer(void* data_pointer)
{
	return (malloc_node_t*)((uint8_t*)data_pointer - sizeof(malloc_node_t));
}

static malloc_pool_t& pool_from_node(malloc_node_t* node)
{
	for (size_t i = 0; i < s_malloc_pool_count; i++)
		if (s_malloc_pools[i].start && s_malloc_pools[i].contains(node))
			return s_malloc_pools[i];
	assert(false);
}

void* malloc(size_t size)
{
	dprintln_if(DEBUG_MALLOC, "malloc({})", size);

	// align size to s_malloc_default_align boundary
	if (size_t ret = size % s_malloc_default_align)
		size += s_malloc_default_align - ret;

	// find the first pool with size atleast size
	size_t first_usable_pool = 0;
	while (s_malloc_pools[first_usable_pool].size - sizeof(malloc_node_t) < size)
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
		// NOTE: always works since we just created the pool
		return allocate_from_pool(i, size);
	}

	errno = ENOMEM;
	return nullptr;
}

void* realloc(void* ptr, size_t size)
{
	dprintln_if(DEBUG_MALLOC, "realloc({}, {})", ptr, size);

	if (ptr == nullptr)
		return malloc(size);

	// align size to s_malloc_default_align boundary
	if (size_t ret = size % s_malloc_default_align)
		size += s_malloc_default_align - ret;

	auto* node = node_from_data_pointer(ptr);
	size_t oldsize = node->data_size();

	if (oldsize == size)
		return ptr;

	// TODO: try to shrink or expand allocation

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
	dprintln_if(DEBUG_MALLOC, "free({})", ptr);

	if (ptr == nullptr)
		return;

	auto* node = node_from_data_pointer(ptr);

	node->allocated = false;

	auto& pool = pool_from_node(node);

	// merge nodes right after freed one
	while (!node->last && !node->next()->allocated)
	{
		auto* next = node->next();
		remove_node_from_pool_free_list(pool, next);
		node->last = next->last;
		node->size += next->size;
	}

	// add node to free list
	if (pool.free_list)
		pool.free_list->prev_free = node;
	node->prev_free = nullptr;
	node->next_free = pool.free_list;
	pool.free_list = node;
}

void* calloc(size_t nmemb, size_t size)
{
	dprintln_if(DEBUG_MALLOC, "calloc({}, {})", nmemb, size);

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
