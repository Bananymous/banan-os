#include <kernel/BootInfo.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>

#include <BAN/Sort.h>

extern uint8_t g_kernel_end[];

namespace Kernel
{

	struct ReservedRegion
	{
		paddr_t paddr;
		uint64_t size;
	};

	static BAN::Vector<ReservedRegion> get_reserved_regions()
	{
		BAN::Vector<ReservedRegion> reserved_regions;
		MUST(reserved_regions.reserve(2 + g_boot_info.modules.size()));
		MUST(reserved_regions.emplace_back(0, 0x100000));
		MUST(reserved_regions.emplace_back(g_boot_info.kernel_paddr, reinterpret_cast<size_t>(g_kernel_end - KERNEL_OFFSET)));
		for (const auto& module : g_boot_info.modules)
			MUST(reserved_regions.emplace_back(module.start, module.size));

		// page align regions
		for (auto& region : reserved_regions)
		{
			const auto rem = region.paddr % PAGE_SIZE;
			region.paddr -= rem;
			region.size += rem;

			if (const auto rem = region.size % PAGE_SIZE)
				region.size += PAGE_SIZE - rem;
		}

		// sort regions
		BAN::sort::sort(reserved_regions.begin(), reserved_regions.end(),
			[](const auto& lhs, const auto& rhs) -> bool {
				if (lhs.paddr == rhs.paddr)
					return lhs.size < rhs.size;
				return lhs.paddr < rhs.paddr;
			}
		);

		// combine overlapping regions
		for (size_t i = 1; i < reserved_regions.size(); i++)
		{
			auto& prev = reserved_regions[i - 1];
			auto& curr = reserved_regions[i - 0];
			if (prev.paddr > curr.paddr + curr.size || curr.paddr > prev.paddr + prev.size)
				continue;
			prev.size = BAN::Math::max(prev.size, curr.paddr + curr.size - prev.paddr);
			reserved_regions.remove(i--);
		}

		return reserved_regions;
	}

	static Heap* s_instance = nullptr;

	void Heap::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new Heap;
		ASSERT(s_instance);
		s_instance->initialize_impl();
	}

	Heap& Heap::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	void Heap::initialize_impl()
	{
		if (g_boot_info.memory_map_entries.empty())
			panic("Bootloader did not provide a memory map");

		auto reserved_regions = get_reserved_regions();
		for (const auto& entry : g_boot_info.memory_map_entries)
		{
			const char* entry_type_string = nullptr;
			switch (entry.type)
			{
				case MemoryMapEntry::Type::Available:
					entry_type_string = "available";
					break;
				case MemoryMapEntry::Type::Reserved:
					entry_type_string = "reserved";
					break;
				case MemoryMapEntry::Type::ACPIReclaim:
					entry_type_string = "acpi reclaim";
					break;
				case MemoryMapEntry::Type::ACPINVS:
					entry_type_string = "acpi nvs";
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			dprintln("{16H}, {16H}, {}",
				entry.address,
				entry.length,
				entry_type_string
			);

			if (entry.type != MemoryMapEntry::Type::Available)
				continue;

			paddr_t e_start = entry.address;
			if (auto rem = e_start % PAGE_SIZE)
				e_start = PAGE_SIZE - rem;

			paddr_t e_end = entry.address + entry.length;
			if (auto rem = e_end % PAGE_SIZE)
				e_end -= rem;

			for (const auto& reserved_region : reserved_regions)
			{
				const paddr_t r_start = reserved_region.paddr;
				const paddr_t r_end   = reserved_region.paddr + reserved_region.size;
				if (r_end < e_start)
					continue;
				if (r_start > e_end)
					break;

				const paddr_t end = BAN::Math::max(e_start, r_start);
				if (e_start + 2 * PAGE_SIZE <= end)
					MUST(m_physical_ranges.emplace_back(e_start, end - e_start));

				e_start = BAN::Math::max(e_start, BAN::Math::min(e_end, r_end));
			}

			if (e_start + 2 * PAGE_SIZE <= e_end)
				MUST(m_physical_ranges.emplace_back(e_start, e_end - e_start));
		}

		uint64_t total_kibi_bytes = 0;
		for (auto& range : m_physical_ranges)
		{
			const uint64_t kibi_bytes = range.usable_memory() / 1024;
			dprintln("RAM {8H}->{8H} ({}.{3} MiB)", range.start(), range.end(), kibi_bytes / 1024, kibi_bytes % 1024);
			total_kibi_bytes += kibi_bytes;
		}
		dprintln("Total RAM {}.{3} MiB", total_kibi_bytes / 1024, total_kibi_bytes % 1024);
	}

	void Heap::release_boot_modules()
	{
		const auto modules = BAN::move(g_boot_info.modules);

		uint64_t kibi_bytes = 0;
		for (const auto& module : modules)
		{
			vaddr_t start = module.start;
			if (auto rem = start % PAGE_SIZE)
				start += PAGE_SIZE - rem;

			vaddr_t end = module.start + module.size;
			if (auto rem = end % PAGE_SIZE)
				end -= rem;

			const size_t size = end - start;
			if (size < 2 * PAGE_SIZE)
				continue;

			SpinLockGuard _(m_lock);
			MUST(m_physical_ranges.emplace_back(start, size));

			kibi_bytes += m_physical_ranges.back().usable_memory() / 1024;
		}

		if (kibi_bytes)
			dprintln("Released {}.{3} MiB of RAM from boot modules", kibi_bytes / 1024, kibi_bytes % 1024);
	}

	paddr_t Heap::take_free_page()
	{
		SpinLockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.free_pages() >= 1)
				return range.reserve_page();
		return 0;
	}

	void Heap::release_page(paddr_t paddr)
	{
		SpinLockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.contains(paddr))
				return range.release_page(paddr);
		panic("tried to free invalid paddr {16H}", paddr);
	}

	paddr_t Heap::take_free_contiguous_pages(size_t pages)
	{
		SpinLockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.free_pages() >= pages)
				if (paddr_t paddr = range.reserve_contiguous_pages(pages))
					return paddr;
		return 0;
	}

	void Heap::release_contiguous_pages(paddr_t paddr, size_t pages)
	{
		SpinLockGuard _(m_lock);
		for (auto& range : m_physical_ranges)
			if (range.contains(paddr))
				return range.release_contiguous_pages(paddr, pages);
		ASSERT_NOT_REACHED();
	}

	size_t Heap::used_pages() const
	{
		SpinLockGuard _(m_lock);
		size_t result = 0;
		for (const auto& range : m_physical_ranges)
			result += range.used_pages();
		return result;
	}

	size_t Heap::free_pages() const
	{
		SpinLockGuard _(m_lock);
		size_t result = 0;
		for (const auto& range : m_physical_ranges)
			result += range.free_pages();
		return result;
	}

}
