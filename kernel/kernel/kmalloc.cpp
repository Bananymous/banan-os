#include <kernel/multiboot.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/Serial.h>

#include <stdint.h>

#define MB (1 << 20)

struct kmalloc_node
{
	uint8_t* addr = nullptr;
	size_t	 size : sizeof(size_t) * 8 - 1;
	size_t   free : 1;
};
static kmalloc_node* s_kmalloc_node_head = nullptr;
static size_t s_kmalloc_node_count;

static uint8_t* const s_kmalloc_node_base = (uint8_t*)0x00200000;
static constexpr size_t s_kmalloc_max_nodes = 1000;

static uint8_t* const s_kmalloc_base = s_kmalloc_node_base + s_kmalloc_max_nodes * sizeof(kmalloc_node);
static constexpr size_t s_kmalloc_size = 1 * MB;
static uint8_t* const s_kmalloc_end = s_kmalloc_base + s_kmalloc_size;

static size_t s_kmalloc_available = 0;
static size_t s_kmalloc_allocated = 0;

void kmalloc_initialize()
{
	if (!(s_multiboot_info->flags & (1 << 6)))
		Kernel::panic("Kmalloc: Bootloader didn't give a memory map");

	// Validate kmalloc memory
	bool valid = false;
	for (size_t i = 0; i < s_multiboot_info->mmap_length;)
	{
		multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(s_multiboot_info->mmap_addr + i);

		if (mmmt->type == 1)
		{
			if (mmmt->base_addr <= (uint64_t)s_kmalloc_base && (uint64_t)s_kmalloc_end <= mmmt->base_addr + mmmt->length)
			{
				dprintln("Total usable RAM: {} MB", (float)mmmt->length / MB);
				valid = true;
				break;
			}
		}

		i += mmmt->size + sizeof(uint32_t);
	}

	if (!valid)
		Kernel::panic("Kmalloc: Could not find 1 MB of memory");

	s_kmalloc_node_count = 1;
	s_kmalloc_node_head = (kmalloc_node*)s_kmalloc_node_base;

	s_kmalloc_allocated = 0;
	s_kmalloc_available = s_kmalloc_size;

	kmalloc_node& head = s_kmalloc_node_head[0];
	head.addr = s_kmalloc_base;
	head.size = s_kmalloc_size;
	head.free = true;
}

void kmalloc_dump_nodes()
{
	dprintln("Kmalloc memory available {} MB", (float)s_kmalloc_available / MB);
	dprintln("Kmalloc memory allocated {} MB", (float)s_kmalloc_allocated / MB);
	dprintln("Using {}/{} nodes", s_kmalloc_node_count, s_kmalloc_max_nodes);
	for (size_t i = 0; i < s_kmalloc_node_count; i++)
	{
		kmalloc_node& node = s_kmalloc_node_head[i];
		dprintln(" ({3}) {}, node at {}, free: {}, size: {}", i, (void*)&node, (void*)node.addr, node.free, node.size);
	}
}

void* kmalloc(size_t size)
{
	// Search for node with free memory and big enough size
	size_t valid_node_index = -1;
	for (size_t i = 0; i < s_kmalloc_node_count; i++)
	{
		kmalloc_node& current = s_kmalloc_node_head[i];
		if (current.free && current.size >= size)
		{
			valid_node_index = i;
			break;
		}
	}

	if (valid_node_index == size_t(-1))
	{
		dprintln("\e[33mKmalloc: Could not allocate {} bytes\e[0m", size);
		return nullptr;
	}

	kmalloc_node& valid_node = s_kmalloc_node_head[valid_node_index];

	// If node's size happens to match requested size,
	// just flip free bit and return the address
	if (valid_node.size == size)
	{
		valid_node.free = false;
		return valid_node.addr;
	}

	if (s_kmalloc_node_count == s_kmalloc_max_nodes)
	{
		dprintln("\e[33mKmalloc: Out of kmalloc nodes\e[0m");
		return nullptr;
	}

	// Shift every node after valid_node one place to right
	for (size_t i = s_kmalloc_node_count - 1; i > valid_node_index; i--)
		s_kmalloc_node_head[i + 1] = s_kmalloc_node_head[i];

	// Create new node after the valid node
	s_kmalloc_node_count++;
	kmalloc_node& new_node = s_kmalloc_node_head[valid_node_index + 1];
	new_node.addr = valid_node.addr + size;
	new_node.size = valid_node.size - size;
	new_node.free = true;
	
	// Update the valid node
	valid_node.size = size;
	valid_node.free = false;

	s_kmalloc_allocated += size;
	s_kmalloc_available -= size;

	return valid_node.addr;
}

void kfree(void* addr)
{
	if (addr == nullptr)
		return;

	// TODO: use binary search etc.

	size_t node_index = -1;
	for (size_t i = 0; i < s_kmalloc_node_count; i++)
	{
		if (s_kmalloc_node_head[i].addr == addr)
		{
			node_index = i;
			break;
		}
	}

	if (node_index == size_t(-1))
	{
		dprintln("\e[33mKmalloc: Attempting to free unallocated pointer {}\e[0m", addr);
		return;
	}


	// Mark this node as free
	kmalloc_node* node = &s_kmalloc_node_head[node_index];
	node->free = true;

	size_t size = node->size;

	// If node before this node is free, merge them
	if (node_index > 0)
	{
		kmalloc_node& prev = s_kmalloc_node_head[node_index - 1];

		if (prev.free)
		{
			prev.size += node->size;

			s_kmalloc_node_count--;
			for (size_t i = node_index; i < s_kmalloc_node_count; i++)
				s_kmalloc_node_head[i] = s_kmalloc_node_head[i + 1];

			node_index--;
			node = &s_kmalloc_node_head[node_index];
		}
	}

	// If node after this node is free, merge them
	if (node_index < s_kmalloc_node_count - 1)
	{
		kmalloc_node& next = s_kmalloc_node_head[node_index + 1];

		if (next.free)
		{
			node->size += next.size;

			s_kmalloc_node_count--;
			for (size_t i = node_index; i < s_kmalloc_node_count; i++)
				s_kmalloc_node_head[i + 1] = s_kmalloc_node_head[i + 2];

			node_index--;
			node = &s_kmalloc_node_head[node_index];
		}
	}

	s_kmalloc_allocated -= size;
	s_kmalloc_available += size;
}
