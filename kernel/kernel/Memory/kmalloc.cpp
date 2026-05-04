#include <kernel/Memory/Heap.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>

static constexpr size_t s_allocator_chunk_size { 64 };
static constexpr size_t s_allocator_align      { alignof(max_align_t) };

static constexpr size_t s_max_allocator_count { 128 };

static constexpr size_t s_allocator_default_size {       128 * 1024 };
static constexpr size_t s_allocator_dynamic_size { 16 * 1024 * 1024 };

alignas(s_allocator_align) static uint8_t s_default_allocator_memory[s_allocator_default_size] {};

// NOTE: 128 KiB + 127 * 16 MiB ~= 2 GiB
//       This is should be more than enough for kmalloc :^)

struct BitmapAllocator
{
	struct Header
	{
		size_t chunks { 0 };
		uint8_t padding[s_allocator_align - sizeof(chunks)];
	};

	uint32_t bitmap_chunks { 0 };
	uint32_t total_chunks  { 0 };
	uint32_t free_chunks   { 0 };
	uint32_t allocations   { 0 };
	uint8_t* base          { nullptr };

	static size_t needed_chunks(size_t size)
	{
		return BAN::Math::div_round_up(sizeof(BitmapAllocator::Header) + size, s_allocator_chunk_size);
	}

	void initialize_default()
	{
		constexpr size_t bitmap_bytes  = BAN::Math::div_round_up(s_allocator_default_size, s_allocator_chunk_size * 8);
		constexpr size_t bitmap_chunks = BAN::Math::div_round_up(bitmap_bytes, s_allocator_chunk_size);
		constexpr size_t usable_chunks = s_allocator_default_size / s_allocator_chunk_size - bitmap_chunks;

		this->bitmap_chunks = bitmap_chunks;
		this->total_chunks  = usable_chunks;
		this->free_chunks   = usable_chunks;
		this->base          = s_default_allocator_memory;

		memset(this->base, 0, bitmap_chunks * s_allocator_chunk_size);
	}

	bool initialize_dynamic()
	{
		using namespace Kernel;

		const size_t page_count = s_allocator_dynamic_size / PAGE_SIZE;

		const vaddr_t vaddr = PageTable::kernel().reserve_free_contiguous_pages(page_count, KERNEL_OFFSET);
		if (vaddr == 0)
			return false;

		for (size_t i = 0; i < page_count; i++)
		{
			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
			{
				for (size_t j = 0; j < i; j++)
					Heap::get().release_page(PageTable::kernel().physical_address_of(vaddr + j * PAGE_SIZE));
				PageTable::kernel().unmap_range(vaddr, page_count * PAGE_SIZE);
				return false;
			}

			PageTable::kernel().map_page_at(paddr, vaddr + i * PAGE_SIZE, PageTable::ReadWrite | PageTable::Present);
		}

		constexpr size_t bitmap_bytes  = BAN::Math::div_round_up(s_allocator_dynamic_size, s_allocator_chunk_size * 8);
		constexpr size_t bitmap_chunks = BAN::Math::div_round_up(bitmap_bytes, s_allocator_chunk_size);
		constexpr size_t usable_chunks = s_allocator_dynamic_size / s_allocator_chunk_size - bitmap_chunks;

		this->bitmap_chunks = bitmap_chunks;
		this->total_chunks  = usable_chunks;
		this->free_chunks   = usable_chunks;
		this->base          = reinterpret_cast<uint8_t*>(vaddr);

		memset(this->base, 0, bitmap_chunks * s_allocator_chunk_size);

		return true;
	}

	uint8_t* data_start() { return base + bitmap_chunks * s_allocator_chunk_size; }
	const uint8_t* data_start() const { return base + bitmap_chunks * s_allocator_chunk_size; }

	size_t get_first_chunk(void* ptr) const
	{
		return (static_cast<uint8_t*>(ptr) - sizeof(Header) - data_start()) / s_allocator_chunk_size;
	}

	bool contains(void* ptr) const
	{
		if (ptr < data_start() + sizeof(Header))
			return false;
		return get_first_chunk(ptr) < total_chunks;
	}

	bool get_bit(size_t index) const
	{
		ASSERT(index < total_chunks);
		const size_t byte = index / 8;
		const size_t bit = index % 8;
		return (base[byte] >> bit) & 1;
	}

	void set_bit(size_t index, bool value)
	{
		ASSERT(index < total_chunks);
		const size_t byte = index / 8;
		const size_t bit = index % 8;
		if (value)
			base[byte] |= 1 << bit;
		else
			base[byte] &= ~(1 << bit);
	}

	size_t find_unset_bit(size_t index) const
	{
		// NOTE: We could optimize other bitmap functions than this
		//       but this one is the bottle neck so it doesn't matter

		static_assert(sizeof(unsigned long long) == sizeof(uint64_t));

		if (index >= total_chunks)
			return index;

		if (const auto rem = index % 64)
		{
			const uint64_t qword = *reinterpret_cast<const uint64_t*>(base + (index - rem) / 8) >> rem;
			if (qword != (1ull << (64 - rem)) - 1)
				return index + __builtin_ctzll(~qword);
			index += 64 - rem;
		}

		while (index < total_chunks)
		{
			const uint64_t qword = *reinterpret_cast<const uint64_t*>(base + index / 8);
			if (qword != UINT64_MAX)
				return index + __builtin_ctzll(~qword);
			index += 64;
		}

		return index;
	}

	size_t count_unset_bits(size_t index, size_t wanted) const
	{
		size_t count = 0;
		for (; index + count < total_chunks && count < wanted; count++)
			if (get_bit(index + count))
				break;
		return count;
	}

	Header& header_from_chunk(size_t index)
	{
		return *reinterpret_cast<Header*>(data_start() + index * s_allocator_chunk_size);
	}

	Header& header_from_ptr(void* ptr)
	{
		return *reinterpret_cast<Header*>(static_cast<uint8_t*>(ptr) - sizeof(Header));
	}

	void* allocate(size_t needed_chunks)
	{
		ASSERT(needed_chunks > 0);

		if (needed_chunks > free_chunks)
			return nullptr;

		for (size_t i = find_unset_bit(0); i <= total_chunks - needed_chunks; i = find_unset_bit(i))
		{
			if (const size_t count = count_unset_bits(i, needed_chunks); count < needed_chunks)
			{
				i += count + 1;
				continue;
			}

			for (size_t j = 0; j < needed_chunks; j++)
				set_bit(i + j, true);

			auto& header = header_from_chunk(i);
			header.chunks = needed_chunks;

			free_chunks -= header.chunks;
			allocations++;

			return &header + 1;
		}

		return nullptr;
	}

	void free(void* ptr)
	{
		ASSERT(contains(ptr));

		const size_t first_chunk = get_first_chunk(ptr);

		auto& header = header_from_ptr(ptr);
		for (size_t i = 0; i < header.chunks; i++)
			set_bit(first_chunk + i, false);

		free_chunks += header.chunks;
		allocations--;
	}
};

static uint8_t s_allocator_storage[s_max_allocator_count * sizeof(BitmapAllocator)];
static BitmapAllocator* s_allocators[s_max_allocator_count] {};

static Kernel::SpinLock s_kmalloc_lock;

void kmalloc_initialize()
{
	auto& allocator = reinterpret_cast<BitmapAllocator*>(s_allocator_storage)[0];
	new (&allocator) BitmapAllocator();
	allocator.initialize_default();
	s_allocators[0] = &allocator;
}

static void kmalloc_dump_info()
{
	ASSERT(s_kmalloc_lock.current_processor_has_lock());

	dwarnln("kmalloc info");
	for (size_t i = 0; i < s_max_allocator_count && s_allocators[i]; i++)
	{
		dwarnln("  allocator {}", i);
		dwarnln("    total size:  {}", s_allocators[i]->total_chunks * s_allocator_chunk_size);
		dwarnln("    free size:   {}", s_allocators[i]->free_chunks  * s_allocator_chunk_size);
		dwarnln("    allocations: {}", s_allocators[i]->allocations);
	}
}

void* kmalloc(size_t size)
{
	const size_t needed_chunks = BitmapAllocator::needed_chunks(size);

	Kernel::SpinLockGuard _(s_kmalloc_lock);

	for (size_t i = 0; i < s_max_allocator_count; i++)
	{
		if (auto* allocator = s_allocators[i])
		{
			if (void* result = allocator->allocate(needed_chunks))
				return result;
			continue;
		}

		auto& new_allocator = reinterpret_cast<BitmapAllocator*>(s_allocator_storage)[i];
		new (&new_allocator) BitmapAllocator();
		if (!new_allocator.initialize_dynamic())
		{
			new_allocator.~BitmapAllocator();
			break;
		}

		s_allocators[i] = &new_allocator;

		if (void* result = new_allocator.allocate(needed_chunks))
			return result;

		break;
	}

	dwarnln("failed to allocate {} bytes", size);
	kmalloc_dump_info();

	return nullptr;
}

void kfree(void* ptr)
{
	if (ptr == nullptr)
		return;

	Kernel::SpinLockGuard _(s_kmalloc_lock);

	for (size_t i = 0; i < s_max_allocator_count && s_allocators[i]; i++)
		if (s_allocators[i]->contains(ptr))
			return s_allocators[i]->free(ptr);

	ASSERT_NOT_REACHED();
}
