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
	bool last;
	size_t size;
	uint8_t data[0];

	size_t data_size() const { return size - sizeof(malloc_node_t); }
	malloc_node_t* next() { return (malloc_node_t*)(data + data_size()); }
};

struct malloc_pool_t
{
	uint8_t* start;
	size_t size;

	malloc_node_t* first_free;

	uint8_t* end() { return start + size; }
	bool contains(malloc_node_t* node) { return start <= (uint8_t*)node && (uint8_t*)node < end(); }
};

static malloc_pool_t s_malloc_pools[s_malloc_pool_count];

void init_malloc()
{
	size_t pool_size = s_malloc_pool_size_initial;
	for (size_t i = 0; i < s_malloc_pool_count; i++)
	{
		s_malloc_pools[i].start = nullptr;
		s_malloc_pools[i].size = pool_size;
		s_malloc_pools[i].first_free = nullptr;
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

	pool.first_free = node;

	return true;
}

static void* allocate_from_pool(size_t pool_index, size_t size)
{
	assert(size % s_malloc_default_align == 0);

	auto& pool = s_malloc_pools[pool_index];
	assert(pool.start != nullptr);

	if (!pool.first_free)
		return nullptr;
	assert(!pool.first_free->allocated);

	for (auto* node = pool.first_free;; node = node->next())
	{
		if (node->allocated)
		{
			if (node->last)
				break;
			continue;
		}

		if (!node->last && !node->next()->allocated)
		{
			node->last = node->next()->last;
			node->size += node->next()->size;
		}
		
		if (node->data_size() < size)
		{
			if (node->last)
				break;
			continue;
		}

		node->allocated = true;

		if (node == pool.first_free)
			pool.first_free = nullptr;

		// shrink node if needed
		if (node->data_size() - size > sizeof(malloc_node_t))
		{
			uint8_t* node_end = (uint8_t*)node->next();

			node->size = sizeof(malloc_node_t) + size;

			auto* next = node->next();
			next->allocated = false;
			next->size = node_end - (uint8_t*)next;
			next->last = node->last;

			node->last = false;

			if (!pool.first_free || next < pool.first_free)
				pool.first_free = next;
		}

		// Find next free node
		if (!pool.first_free)
		{
			for (auto* free_node = node;; free_node = free_node->next())
			{
				if (!free_node->allocated)
				{
					pool.first_free = free_node;
					break;
				}
				if (free_node->last)
					break;
			}
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
			next->last = node->last;

			node->last = false;

			auto& pool = pool_from_node(node);
			if (!pool.first_free || next < pool.first_free)
				pool.first_free = next;
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
	if (!node->last && !node->next()->allocated)
	{
		node->last = node->next()->last;
		node->size += node->next()->size;
	}

	auto& pool = pool_from_node(node);
	if (!pool.first_free || node < pool.first_free)
		pool.first_free = node;
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
