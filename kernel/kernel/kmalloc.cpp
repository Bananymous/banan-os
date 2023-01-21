#include <BAN/Errors.h>
#include <BAN/Math.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>

#include <stdint.h>

#define MB (1 << 20)

/*

Kmalloc holds a bitmap of free/allocated chunks

When allocating n chunks, kmalloc will put the number of chunks
to address, and return pointer to the byte after the stored size

*/

static constexpr uintptr_t	s_kmalloc_base	= 0x00200000;
static constexpr size_t		s_kmalloc_size	= 1 * MB;
static constexpr uintptr_t	s_kmalloc_end	= s_kmalloc_base + s_kmalloc_size;

static constexpr uintptr_t	s_kmalloc_eternal_base	= s_kmalloc_end;
static constexpr size_t		s_kmalloc_eternal_size	= 1 * MB;
static constexpr uintptr_t	s_kmalloc_eternal_end	= s_kmalloc_eternal_base + s_kmalloc_eternal_size;

static constexpr size_t		s_kmalloc_default_align		= alignof(max_align_t);
static constexpr size_t		s_kmalloc_chunk_size		= s_kmalloc_default_align;
static constexpr size_t		s_kmalloc_chunks_per_size	= sizeof(size_t) * 8 / s_kmalloc_chunk_size;
static constexpr size_t		s_kmalloc_total_chunks		= s_kmalloc_size / s_kmalloc_chunk_size;
static			 uint8_t	s_kmalloc_bitmap[s_kmalloc_total_chunks / 8] { 0 };

static			size_t		s_kmalloc_free = s_kmalloc_size;
static			size_t		s_kmalloc_used = 0;

static			size_t		s_kmalloc_eternal_free = s_kmalloc_eternal_size;
static			size_t		s_kmalloc_eternal_used = 0;

extern "C" uintptr_t g_kernel_end;

static bool is_kmalloc_chunk_used(size_t index)
{
	ASSERT(index < s_kmalloc_total_chunks);
	return s_kmalloc_bitmap[index / 8] & (1 << (index % 8));
}

static uintptr_t chunk_address(size_t index)
{
	ASSERT(index < s_kmalloc_total_chunks);
	return s_kmalloc_base + s_kmalloc_chunk_size * index;
}

void kmalloc_initialize()
{
	if (!(g_multiboot_info->flags & (1 << 6)))
		Kernel::Panic("Kmalloc: Bootloader didn't provide a memory map");

	if ((uintptr_t)&g_kernel_end > s_kmalloc_base)
		Kernel::Panic("Kmalloc: Kernel end ({}) is over kmalloc base ({})", &g_kernel_end, (void*)s_kmalloc_base);

	// Validate kmalloc memory
	bool valid = false;
	for (size_t i = 0; i < g_multiboot_info->mmap_length;)
	{
		multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(g_multiboot_info->mmap_addr + i);

		if (mmmt->type == 1)
		{
			if (mmmt->base_addr <= s_kmalloc_base && s_kmalloc_eternal_end <= mmmt->base_addr + mmmt->length)
			{
				dprintln("Total usable RAM: {}.{} MB", mmmt->length / MB, mmmt->length % MB);
				valid = true;
				break;
			}
		}

		i += mmmt->size + sizeof(uint32_t);
	}

	if (!valid)
	{
		Kernel::Panic("Kmalloc: Could not find {}.{} MB of memory",
			(s_kmalloc_eternal_end - s_kmalloc_base) / MB,
			(s_kmalloc_eternal_end - s_kmalloc_base) % MB
		);
	}
}

void kmalloc_dump_info()
{
	kprintln("kmalloc: {}->{}", (void*)s_kmalloc_base, (void*)s_kmalloc_end);
	kprintln("  used: {}", s_kmalloc_used);
	kprintln("  free: {}", s_kmalloc_free);

	kprintln("kmalloc eternal: {}->{}", (void*)s_kmalloc_eternal_base, (void*)s_kmalloc_eternal_end);
	kprintln("  used: {}", s_kmalloc_eternal_used);
	kprintln("  free: {}", s_kmalloc_eternal_free);
}

void* kmalloc(size_t size)
{
	return kmalloc(size, s_kmalloc_default_align);
}

void* kmalloc(size_t size, size_t align)
{
	if (size == 0 || size >= s_kmalloc_size)
		return nullptr;

	if (align == 0)
		align = s_kmalloc_chunk_size;
	
	if (align < s_kmalloc_chunk_size || align % s_kmalloc_chunk_size)
	{
		size_t new_align = BAN::Math::lcm(align, s_kmalloc_chunk_size);
		dwarnln("kmalloc asked to align to {}, aliging to {} instead", align, new_align);
		align = new_align;
	}

	size_t needed_chunks = (size - 1) / s_kmalloc_chunk_size + 1 + s_kmalloc_chunks_per_size;
	for (size_t i = 0; i < s_kmalloc_total_chunks - needed_chunks; i++)
	{
		if (chunk_address(i + s_kmalloc_chunks_per_size) % align)
			continue;

		bool free = true;
		for (size_t j = 0; j < needed_chunks; j++)
		{
			if (is_kmalloc_chunk_used(i + j))
			{
				free = false;
				i += j;
				break;
			}
		}
		if (free)
		{
			*(size_t*)chunk_address(i) = needed_chunks;
			for (size_t j = 0; j < needed_chunks; j++)
				s_kmalloc_bitmap[(i + j) / 8] |= (1 << ((i + j) % 8));
			s_kmalloc_used += needed_chunks * s_kmalloc_chunk_size;
			s_kmalloc_free -= needed_chunks * s_kmalloc_chunk_size;
			return (void*)chunk_address(i + s_kmalloc_chunks_per_size);
		}
	}

	dwarnln("Could not allocate {} bytes", size);
	return nullptr;
}

void kfree(void* address)
{
	if (!address)
		return;
	ASSERT(((uintptr_t)address % s_kmalloc_chunk_size) == 0);
	ASSERT(s_kmalloc_base <= (uintptr_t)address && (uintptr_t)address < s_kmalloc_end);

	size_t first_chunk = ((uintptr_t)address - s_kmalloc_base) / s_kmalloc_chunk_size - s_kmalloc_chunks_per_size;
	ASSERT(is_kmalloc_chunk_used(first_chunk));

	size_t size = *(size_t*)chunk_address(first_chunk);
	for (size_t i = 0; i < size; i++)
		s_kmalloc_bitmap[(first_chunk + i) / 8] &= ~(1 << ((first_chunk + i) % 8));
	s_kmalloc_used -= size * s_kmalloc_chunk_size;
	s_kmalloc_free += size * s_kmalloc_chunk_size;
}