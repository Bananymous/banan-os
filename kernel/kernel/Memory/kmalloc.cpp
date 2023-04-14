#include <BAN/Errors.h>
#include <kernel/CriticalScope.h>
#include <kernel/kprint.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/multiboot.h>

#include <kernel/Thread.h>

#define MB (1 << 20)

static constexpr size_t s_kmalloc_min_align = alignof(max_align_t);

struct kmalloc_node
{
	void set_align(ptrdiff_t align)			{ m_align = align; }
	void set_end(uintptr_t end)				{ m_size = end - (uintptr_t)m_data; }
	void set_used(bool used)				{ m_used = used; }

	bool can_align(uint32_t align)			{ return align < m_size; }
	bool can_fit_before()					{ return m_align > sizeof(kmalloc_node); }
	bool can_fit_after(size_t new_size)		{ return data() + new_size < end() - sizeof(kmalloc_node); }

	void split_in_align()
	{
		uintptr_t node_end = end();
		set_end(data() - sizeof(kmalloc_node));
		set_align(0);

		auto* next = after();
		next->set_end(node_end);
		next->set_align(0);
	}

	void split_after_size(size_t size)
	{
		uintptr_t node_end = end();
		set_end(data() + size);
		
		auto* next = after();
		next->set_end(node_end);
		next->set_align(0);
	}

	bool used()					{ return m_used; }
	uintptr_t size_no_align()	{ return m_size; }
	uintptr_t size()			{ return size_no_align() - m_align; }
	uintptr_t data_no_align()	{ return (uintptr_t)m_data; }
	uintptr_t data()			{ return data_no_align() + m_align; }
	uintptr_t end()				{ return data_no_align() + m_size; }
	kmalloc_node* after()		{ return (kmalloc_node*)end(); }

private:
	uint32_t	m_size;
	uint32_t	m_align;
	bool		m_used;
	uint8_t 	m_padding[s_kmalloc_min_align - sizeof(m_size) - sizeof(m_align) - sizeof(m_used)];
	uint8_t		m_data[0];
};
static_assert(sizeof(kmalloc_node) == s_kmalloc_min_align);

struct kmalloc_info
{
	static constexpr uintptr_t	base = 0x00400000;
	static constexpr size_t		size = 1 * MB;
	static constexpr uintptr_t	end = base + size;

	kmalloc_node* first() { return (kmalloc_node*)base; }
	kmalloc_node* from_address(void* addr)
	{
		for (auto* node = first(); node->end() < end; node = node->after())
			if (node->data() == (uintptr_t)addr)
				return node;
		return nullptr;
	}

	size_t used = 0;
	size_t free = size;
};
static kmalloc_info s_kmalloc_info;

template<size_t SIZE>
struct kmalloc_fixed_node
{
	uint8_t data[SIZE - 2 * sizeof(uint16_t)];
	uint16_t prev = NULL;
	uint16_t next = NULL;
	static constexpr uint16_t invalid = ~0;
};

struct kmalloc_fixed_info
{
	using node = kmalloc_fixed_node<64>;

	static constexpr uintptr_t	base = s_kmalloc_info.end;
	static constexpr size_t		size = 1 * MB;
	static constexpr uintptr_t	end = base + size;
	static constexpr size_t		node_count = size / sizeof(node);
	static_assert(node_count < (1 << 16));

	node* free_list_head = NULL;
	node* used_list_head = NULL;

	node* node_at(size_t index) { return (node*)(base + index * sizeof(node)); }
	uint16_t index_of(const node* p) { return ((uintptr_t)p - base) / sizeof(node); }

	size_t used = 0;
	size_t free = size;
};
static kmalloc_fixed_info s_kmalloc_fixed_info;

extern "C" uintptr_t g_kernel_end;

void kmalloc_initialize()
{
	if (!(g_multiboot_info->flags & (1 << 6)))
		Kernel::panic("Kmalloc: Bootloader didn't provide a memory map");

	if ((uintptr_t)&g_kernel_end > s_kmalloc_info.base)
		Kernel::panic("Kmalloc: Kernel end ({}) is over kmalloc base ({})", &g_kernel_end, (void*)s_kmalloc_info.base);

	// Validate kmalloc memory
	bool valid = false;
	for (size_t i = 0; i < g_multiboot_info->mmap_length;)
	{
		multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(g_multiboot_info->mmap_addr + i);

		if (mmmt->type == 1)
		{
			if (mmmt->base_addr <= s_kmalloc_info.base && s_kmalloc_fixed_info.end <= mmmt->base_addr + mmmt->length)
			{
				valid = true;
				break;
			}
		}

		i += mmmt->size + sizeof(uint32_t);
	}

	if (!valid)
	{
		size_t kmalloc_total_size = s_kmalloc_info.size + s_kmalloc_fixed_info.size;
		Kernel::panic("Kmalloc: Could not find {}.{} MB of memory",
			kmalloc_total_size / MB,
			kmalloc_total_size % MB
		);
	}

	// initialize fixed size allocations
	{
		auto& info = s_kmalloc_fixed_info;

		for (size_t i = 0; i < info.node_count; i++)
		{
			auto* node = info.node_at(i);
			node->next = i - 1;
			node->prev = i + 1;
		}
		
		info.node_at(0)->next = kmalloc_fixed_info::node::invalid;
		info.node_at(info.node_count - 1)->prev = kmalloc_fixed_info::node::invalid;
		
		info.free_list_head = info.node_at(0);
		info.used_list_head = nullptr;
	}

	// initial general allocations
	{
		auto& info = s_kmalloc_info;
		auto* node = info.first();
		node->set_end(info.end);
		node->set_align(0);
		node->set_used(false);
	}
}

void kmalloc_dump_info()
{
	kprintln("kmalloc:               0x{8H}->0x{8H}", s_kmalloc_info.base, s_kmalloc_info.end);
	kprintln("  used: 0x{8H}", s_kmalloc_info.used);
	kprintln("  free: 0x{8H}", s_kmalloc_info.free);

	kprintln("kmalloc fixed {} byte: 0x{8H}->0x{8H}", sizeof(kmalloc_fixed_info::node), s_kmalloc_fixed_info.base, s_kmalloc_fixed_info.end);
	kprintln("  used: 0x{8H}", s_kmalloc_fixed_info.used);
	kprintln("  free: 0x{8H}", s_kmalloc_fixed_info.free);
}

static bool is_corrupted()
{
	auto& info = s_kmalloc_info;
	auto* temp = info.first();
	for (; temp->end() <= info.end; temp = temp->after());
	return (uintptr_t)temp != info.end;
}

static void debug_dump()
{
	auto& info = s_kmalloc_info;

	uint32_t used = 0;
	uint32_t free = 0;

	for (auto* node = info.first(); node->data() <= info.end; node = node->after())
	{
		(node->used() ? used : free) += sizeof(kmalloc_node) + node->size_no_align();
		dprintln("{} node {H} -> {H}", node->used() ? "used" : "free", node->data(), node->end());
	}

	dprintln("total used: {}", used);
	dprintln("total free: {}", free);
	dprintln("            {}", used + free);
}

static void* kmalloc_fixed()
{
	auto& info = s_kmalloc_fixed_info;

	if (!info.free_list_head)
		return nullptr;

	// allocate the node on top of free list
	auto* node = info.free_list_head;
	ASSERT(node->next == kmalloc_fixed_info::node::invalid);
	
	// remove the node from free list
	if (info.free_list_head->prev != kmalloc_fixed_info::node::invalid)
	{
		info.free_list_head = info.node_at(info.free_list_head->prev);
		info.free_list_head->next = kmalloc_fixed_info::node::invalid;
	}
	else
	{
		derrorln("removing free list, allocated {}", info.used);
		info.free_list_head = nullptr;
	}
	node->prev = kmalloc_fixed_info::node::invalid;
	node->next = kmalloc_fixed_info::node::invalid;

	// move the node to the top of used nodes
	if (info.used_list_head)
	{
		info.used_list_head->next = info.index_of(node);
		node->prev = info.index_of(info.used_list_head);
	}
	info.used_list_head = node;

	info.used += sizeof(kmalloc_fixed_info::node);
	info.free -= sizeof(kmalloc_fixed_info::node);

	return (void*)node->data;
}

static void* kmalloc_impl(size_t size, size_t align)
{
	ASSERT(align % s_kmalloc_min_align == 0);
	ASSERT(size % s_kmalloc_min_align == 0);

	auto& info = s_kmalloc_info;

	for (auto* node = info.first(); node->end() <= info.end; node = node->after())
	{
		if (node->used())
			continue;

		if (auto* next = node->after(); next->end() <= info.end)
			if (!next->used())
				node->set_end(next->end());

		if (node->size_no_align() < size)
			continue;

		ptrdiff_t needed_align = 0;
		if (ptrdiff_t rem = node->data_no_align() % align)
			needed_align = align - rem;

		if (!node->can_align(needed_align))
			continue;

		node->set_align(needed_align);
		ASSERT(node->data() % align == 0);

		if (node->size() < size)
			continue;

		if (node->can_fit_before())
		{
			node->split_in_align();
			node->set_used(false);

			node = node->after();
			ASSERT(node->data() % align == 0);
		}

		node->set_used(true);

		if (node->can_fit_after(size))
		{
			node->split_after_size(size);
			node->after()->set_used(false);
			ASSERT(node->data() % align == 0);
		}

		info.used += sizeof(kmalloc_node) + node->size_no_align();
		info.free -= sizeof(kmalloc_node) + node->size_no_align();

		return (void*)node->data();
	}

	return nullptr;
}

void* kmalloc(size_t size)
{
	return kmalloc(size, s_kmalloc_min_align);
}

static constexpr bool is_power_of_two(size_t value)
{
	if (value == 0)
		return false;
	return (value & (value - 1)) == 0;
}

void* kmalloc(size_t size, size_t align)
{
	const kmalloc_info& info = s_kmalloc_info;

	if (size == 0 || size >= info.size)
		return nullptr;

	ASSERT(is_power_of_two(align));
	if (align < s_kmalloc_min_align)
		align = s_kmalloc_min_align;
	
	Kernel::CriticalScope critical;

	// if the size fits into fixed node, we will try to use that since it is faster
	if (align == s_kmalloc_min_align && size <= sizeof(kmalloc_fixed_info::node::data))
		if (void* result = kmalloc_fixed())
			return result;

	if (ptrdiff_t rem = size % s_kmalloc_min_align)
		size += s_kmalloc_min_align - rem;
	
	if (void* res = kmalloc_impl(size, align))
		return res;

	dwarnln("could not allocate {H} bytes ({} aligned)", size, align);
	dwarnln(" {6H} free (fixed)", s_kmalloc_fixed_info.free);
	dwarnln(" {6H} free", s_kmalloc_info.free);
	debug_dump();
	Debug::dump_stack_trace();
	ASSERT(!is_corrupted());

	return nullptr;
}

void kfree(void* address)
{
	if (address == nullptr)
		return;

	uintptr_t address_uint = (uintptr_t)address;
	ASSERT(address_uint % s_kmalloc_min_align == 0);

	Kernel::CriticalScope critical;

	if (s_kmalloc_fixed_info.base <= address_uint && address_uint < s_kmalloc_fixed_info.end)
	{
		auto& info = s_kmalloc_fixed_info;
		ASSERT(info.used_list_head);

		// get node from fixed info buffer
		auto* node = (kmalloc_fixed_info::node*)address;
		ASSERT(node->next < info.node_count || node->next == kmalloc_fixed_info::node::invalid);
		ASSERT(node->prev < info.node_count || node->prev == kmalloc_fixed_info::node::invalid);

		// remove from used list
		if (node->prev != kmalloc_fixed_info::node::invalid)
			info.node_at(node->prev)->next = node->next;
		if (node->next != kmalloc_fixed_info::node::invalid)
			info.node_at(node->next)->prev = node->prev;
		if (info.used_list_head == node)
			info.used_list_head = info.used_list_head->prev != kmalloc_fixed_info::node::invalid ? info.node_at(info.used_list_head->prev) : nullptr;

		// add to free list
		node->next = kmalloc_fixed_info::node::invalid;
		node->prev = kmalloc_fixed_info::node::invalid;
		if (info.free_list_head)
		{
			info.free_list_head->next = info.index_of(node);
			node->prev = info.index_of(info.free_list_head);
		}
		info.free_list_head = node;

		info.used -= sizeof(kmalloc_fixed_info::node);
		info.free += sizeof(kmalloc_fixed_info::node);
	}
	else if (s_kmalloc_info.base <= address_uint && address_uint < s_kmalloc_info.end)
	{
		auto& info = s_kmalloc_info;
		
		auto* node = info.from_address(address);
		ASSERT(node);
		ASSERT(node->data() == (uintptr_t)address);
		ASSERT(node->used());

		ptrdiff_t size = node->size_no_align();

		if (auto* next = node->after(); next->end() <= info.end)
			if (!next->used())
				node->set_end(node->after()->end());
		node->set_used(false);

		info.used -= sizeof(kmalloc_node) + size;
		info.free += sizeof(kmalloc_node) + size;
	}
	else
	{
		Kernel::panic("Trying to free a pointer outsize of kmalloc memory");
	}

}