#include <BAN/Atomic.h>
#include <BAN/Math.h>

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static constexpr size_t s_allocator_chunk_size { 64 };
static constexpr size_t s_allocator_size       { 1024 * 1024 };
static constexpr size_t s_mmap_threshold       {   64 * 1024 };

struct alignas(max_align_t) MmapAllocationHeader
{
	size_t mmap_size;
	size_t offset;
};

struct BitmapAllocator
{
	struct alignas(max_align_t) Header
	{
		size_t chunks { 0 };
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

	bool initialize()
	{
		void* base = mmap(nullptr, s_allocator_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (base == MAP_FAILED)
			return false;

		constexpr size_t bitmap_bytes  = BAN::Math::div_round_up(s_allocator_size, s_allocator_chunk_size * 8);
		constexpr size_t bitmap_chunks = BAN::Math::div_round_up(bitmap_bytes, s_allocator_chunk_size);
		constexpr size_t usable_chunks = s_allocator_size / s_allocator_chunk_size - bitmap_chunks;

		this->bitmap_chunks = bitmap_chunks;
		this->total_chunks  = usable_chunks;
		this->free_chunks   = usable_chunks;
		this->base          = static_cast<uint8_t*>(base);

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
		assert(index < total_chunks);
		const size_t byte = index / 8;
		const size_t bit = index % 8;
		return (base[byte] >> bit) & 1;
	}

	void set_bit(size_t index, bool value)
	{
		assert(index < total_chunks);
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

		if (index >= total_chunks)
			return index;

		if (const auto rem = index % 64)
		{
			const uint64_t qword = *reinterpret_cast<const uint64_t*>(base + (index - rem) / 8) >> rem;
			if (qword != UINT64_MAX >> rem)
				return index + BAN::Math::ctz(~qword);
			index += 64 - rem;
		}

		while (index < total_chunks)
		{
			const uint64_t qword = *reinterpret_cast<const uint64_t*>(base + index / 8);
			if (qword != UINT64_MAX)
				return index + BAN::Math::ctz(~qword);
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

	void* allocate(size_t size)
	{
		const size_t needed_chunks = this->needed_chunks(size);
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
			header = {
				.chunks = needed_chunks,
			};

			free_chunks -= header.chunks;
			allocations++;

			return &header + 1;
		}

		return nullptr;
	}

	bool resize(void* ptr, size_t size)
	{
		assert(contains(ptr));

		const size_t first_chunk = get_first_chunk(ptr);
		auto& header = header_from_chunk(first_chunk);

		const size_t needed_chunks = this->needed_chunks(size);
		if (needed_chunks <= header.chunks)
		{
			for (size_t i = needed_chunks; i < header.chunks; i++)
				set_bit(first_chunk + i, false);
			free_chunks += header.chunks - needed_chunks;
		}
		else
		{
			const size_t extra_chunks = header.chunks - needed_chunks;
			if (count_unset_bits(first_chunk + header.chunks, extra_chunks) < extra_chunks)
				return false;
			for (size_t i = header.chunks; i < needed_chunks; i++)
				set_bit(first_chunk + i, true);
			free_chunks -= needed_chunks - header.chunks;
		}

		header = {
			.chunks = needed_chunks,
		};
		return true;
	}

	void free(void* ptr)
	{
		assert(contains(ptr));

		const size_t first_chunk = get_first_chunk(ptr);

		auto& header = header_from_chunk(first_chunk);
		for (size_t i = 0; i < header.chunks; i++)
			set_bit(first_chunk + i, false);

		free_chunks += header.chunks;
		allocations--;
	}

	size_t allocation_size(void* ptr)
	{
		assert(contains(ptr));
		return header_from_chunk(get_first_chunk(ptr)).chunks * s_allocator_chunk_size - sizeof(Header);
	}
};

static size_t s_allocator_count { 0 };
static size_t s_allocator_capacity { 0 };

static BitmapAllocator* s_allocators { nullptr };
static pthread_mutex_t s_allocator_lock = PTHREAD_MUTEX_INITIALIZER;

static BAN::Atomic<size_t> s_mmap_count { 0 };
static BAN::Atomic<size_t> s_mmap_bytes { 0 };

void* malloc(size_t total_size)
{
	if (total_size >= s_mmap_threshold)
	{
		const size_t page_size = getpagesize();

		size_t mmap_size = sizeof(MmapAllocationHeader) + total_size;
		if (const auto rem = mmap_size % page_size)
			mmap_size += page_size - rem;

		void* address = mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (address == MAP_FAILED)
			return nullptr;

		s_mmap_count++;
		s_mmap_bytes += mmap_size;

		auto& header = static_cast<MmapAllocationHeader*>(address)[0];
		header = {
			.mmap_size = mmap_size,
			.offset = sizeof(MmapAllocationHeader),
		};
		return &header + 1;
	}

	void* result = nullptr;

	pthread_mutex_lock(&s_allocator_lock);

	for (size_t i = 0; i < s_allocator_count; i++)
		if ((result = s_allocators[i].allocate(total_size)))
			goto malloc_return;

	if (s_allocator_count == s_allocator_capacity)
	{
		const size_t page_size = getpagesize();
		const size_t allocator_pages = (s_allocator_capacity * sizeof(BitmapAllocator) + page_size - 1) / page_size;
		const size_t new_allocator_pages = allocator_pages + 1;

		void* new_allocators = mmap(nullptr, new_allocator_pages * page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (new_allocators == MAP_FAILED)
			goto malloc_return;

		if (s_allocators != nullptr)
		{
			static_assert(BAN::is_trivially_copyable_v<BitmapAllocator>);
			memcpy(new_allocators, s_allocators, s_allocator_count * sizeof(BitmapAllocator));
			munmap(s_allocators, s_allocator_capacity * sizeof(BitmapAllocator));
		}

		s_allocators = static_cast<BitmapAllocator*>(new_allocators);
		s_allocator_capacity = new_allocator_pages * page_size / sizeof(BitmapAllocator);
	}

	if (!s_allocators[s_allocator_count].initialize())
		goto malloc_return;
	result = s_allocators[s_allocator_count++].allocate(total_size);

malloc_return:
	pthread_mutex_unlock(&s_allocator_lock);

	if (result == nullptr)
		errno = ENOMEM;
	return result;
}

void free(void* ptr)
{
	if (ptr == nullptr)
		return;

	pthread_mutex_lock(&s_allocator_lock);
	for (size_t i = 0; i < s_allocator_count; i++)
	{
		if (!s_allocators[i].contains(ptr))
			continue;
		s_allocators[i].free(ptr);
		pthread_mutex_unlock(&s_allocator_lock);
		return;
	}
	pthread_mutex_unlock(&s_allocator_lock);

	const auto& header = static_cast<MmapAllocationHeader*>(ptr)[-1];
	s_mmap_count--;
	s_mmap_bytes -= header.mmap_size;
	munmap(static_cast<uint8_t*>(ptr) - header.offset, header.mmap_size);
}

void* realloc(void* ptr, size_t size)
{
	if (ptr == nullptr)
		return malloc(size);

	bool was_mmapped = true;
	size_t old_alloc_size = 0;

	pthread_mutex_lock(&s_allocator_lock);
	for (size_t i = 0; i < s_allocator_count; i++)
	{
		if (!s_allocators[i].contains(ptr))
			continue;
		if (s_allocators[i].resize(ptr, size))
		{
			pthread_mutex_unlock(&s_allocator_lock);
			return ptr;
		}
		was_mmapped = false;
		old_alloc_size = s_allocators[i].allocation_size(ptr);
		break;
	}
	pthread_mutex_unlock(&s_allocator_lock);

	if (was_mmapped)
	{
		auto& header = static_cast<MmapAllocationHeader*>(ptr)[-1];

		old_alloc_size = header.mmap_size - header.offset;

		if (size == old_alloc_size)
			return ptr;

		const size_t page_size = getpagesize();

		size_t new_mmap_size = header.offset + size;
		if (const auto rem = new_mmap_size % page_size)
			new_mmap_size += page_size - rem;

		if (size < old_alloc_size)
		{
			// TODO: should we drop back to bitmap based allocations if the new size is < mmap threshold?
			const size_t shrink_bytes = header.mmap_size - new_mmap_size;
			munmap(static_cast<uint8_t*>(ptr) - header.offset + new_mmap_size, shrink_bytes);
			s_mmap_bytes -= shrink_bytes;
			header.mmap_size = new_mmap_size;
			return ptr;
		}

		const size_t extend_bytes = new_mmap_size - header.mmap_size;

		void* extend_data = mmap(
			static_cast<uint8_t*>(ptr) - header.offset + header.mmap_size,
			extend_bytes,
			PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE,
			-1, 0
		);

		if (extend_data != MAP_FAILED)
		{
			s_mmap_bytes += extend_bytes;
			header.mmap_size = new_mmap_size;
			return ptr;
		}
	}

	void* new_ptr = malloc(size);
	if (new_ptr == nullptr)
		return nullptr;

	memcpy(new_ptr, ptr, BAN::Math::min(old_alloc_size, size));

	free(ptr);

	return new_ptr;
}

void* calloc(size_t nmemb, size_t size)
{
	if (BAN::Math::will_multiplication_overflow(nmemb, size))
	{
		errno = ENOMEM;
		return nullptr;
	}

	const size_t total = nmemb * size;

	void* ptr = malloc(total);
	if (ptr == nullptr)
		return nullptr;

	if (total < s_mmap_threshold)
		memset(ptr, 0, total);
	return ptr;
}

void* reallocarray(void* ptr, size_t nmemb, size_t size)
{
	if (BAN::Math::will_multiplication_overflow(nmemb, size))
	{
		errno = ENOMEM;
		return nullptr;
	}

	return realloc(ptr, nmemb * size);
}

void* aligned_alloc(size_t alignment, size_t size)
{
	if (!BAN::Math::is_power_of_two(alignment))
	{
		errno = EINVAL;
		return nullptr;
	}

	if (alignment <= alignof(max_align_t))
		return malloc(size);

	static_assert(sizeof(MmapAllocationHeader) <= alignof(max_align_t));

	const size_t page_size = getpagesize();

	size_t mmap_size = alignment + size;
	if (const auto rem = mmap_size % page_size)
		mmap_size += page_size - rem;

	void* addr_ptr = mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (addr_ptr == MAP_FAILED)
		return nullptr;

	uintptr_t addr = reinterpret_cast<uintptr_t>(addr_ptr);

	uintptr_t data = addr + sizeof(MmapAllocationHeader);
	if (const auto rem = data % alignment)
		data += alignment - rem;

	if ((data - sizeof(MmapAllocationHeader)) - addr >= page_size)
	{
		const size_t pages_to_free = ((data - sizeof(MmapAllocationHeader)) - addr) / page_size;
		munmap(addr_ptr, pages_to_free * page_size);
		mmap_size -= pages_to_free * page_size;
		addr += pages_to_free * page_size;
	}

	s_mmap_count++;
	s_mmap_bytes += mmap_size;

	auto& header = reinterpret_cast<MmapAllocationHeader*>(data)[-1];
	header = {
		.mmap_size = mmap_size,
		.offset = data - addr,
	};
	return &header + 1;
}

int posix_memalign(void** memptr, size_t alignment, size_t size)
{
	if (alignment < sizeof(void*) || !BAN::Math::is_power_of_two(alignment))
		return EINVAL;
	return (*memptr = aligned_alloc(alignment, size)) ? 0 : errno;
}

size_t malloc_usable_size(void* ptr)
{
	if (ptr == nullptr)
		return 0;

	pthread_mutex_lock(&s_allocator_lock);
	for (size_t i = 0; i < s_allocator_count; i++)
	{
		if (!s_allocators[i].contains(ptr))
			continue;
		const size_t size = s_allocators[i].allocation_size(ptr);
		pthread_mutex_unlock(&s_allocator_lock);
		return size;
	}
	pthread_mutex_unlock(&s_allocator_lock);

	const auto& header = static_cast<MmapAllocationHeader*>(ptr)[-1];
	return header.mmap_size - header.offset;
}

struct mallinfo mallinfo(void)
{
	constexpr auto saturate = [](size_t value) -> int {
		if (value > static_cast<size_t>(BAN::numeric_limits<int>::max()))
			return BAN::numeric_limits<int>::max();
		return value;
	};

	const auto info = mallinfo2();
	return {
		.arena    = saturate(info.arena),
		.ordblks  = saturate(info.ordblks),
		.smblks   = saturate(info.smblks),
		.hblks    = saturate(info.hblks),
		.hblkhd   = saturate(info.hblkhd),
		.usmblks  = saturate(info.usmblks),
		.fsmblks  = saturate(info.fsmblks),
		.uordblks = saturate(info.uordblks),
		.fordblks = saturate(info.fordblks),
		.keepcost = saturate(info.keepcost),
	};
}

struct mallinfo2 mallinfo2(void)
{
	struct mallinfo2 info {};

	pthread_mutex_lock(&s_allocator_lock);
	info.arena = s_allocator_count * s_allocator_size;
	for (size_t i = 0; i < s_allocator_count; i++)
	{
		const size_t total_mem = s_allocators[i].total_chunks * s_allocator_chunk_size;
		const size_t free_mem  = s_allocators[i].free_chunks  * s_allocator_chunk_size;
		info.fordblks += free_mem;
		info.uordblks += total_mem - free_mem;
	}
	pthread_mutex_unlock(&s_allocator_lock);

	info.hblks     = s_mmap_count.load();
	info.hblkhd    = s_mmap_bytes.load();
	info.uordblks += info.hblkhd;

	return info;
}
