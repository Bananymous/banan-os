#pragma once

#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	class MemoryBackedRegion final : public MemoryRegion
	{
		BAN_NON_COPYABLE(MemoryBackedRegion);
		BAN_NON_MOVABLE(MemoryBackedRegion);
	
	public:
		static BAN::ErrorOr<BAN::UniqPtr<MemoryBackedRegion>> create(PageTable&, size_t size, AddressRange, Type, PageTable::flags_t);
		~MemoryBackedRegion();

		virtual BAN::ErrorOr<bool> allocate_page_containing(vaddr_t vaddr) override;

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) override;

		// Copy data from buffer into this region
		// This can fail if no memory is mapped and no free memory was available
		BAN::ErrorOr<void> copy_data_to_region(size_t offset_into_region, const uint8_t* buffer, size_t buffer_size);

	private:
		MemoryBackedRegion(PageTable&, size_t size, Type, PageTable::flags_t);
	};

}