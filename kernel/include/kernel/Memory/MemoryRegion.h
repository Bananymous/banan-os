#pragma once

#include <BAN/UniqPtr.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Memory/Types.h>

#include <stddef.h>

namespace Kernel
{

	struct AddressRange
	{
		vaddr_t start;
		vaddr_t end;
	};

	class MemoryRegion
	{
		BAN_NON_COPYABLE(MemoryRegion);
		BAN_NON_MOVABLE(MemoryRegion);

	public:
		enum class Type : uint8_t
		{
			PRIVATE,
			SHARED
		};

	public:
		virtual ~MemoryRegion();

		bool contains(vaddr_t address) const;
		bool contains_fully(vaddr_t address, size_t size) const;
		bool overlaps(vaddr_t address, size_t size) const;

		size_t size() const { return m_size; }
		vaddr_t vaddr() const { return m_vaddr; }

		size_t virtual_page_count() const { return BAN::Math::div_round_up<size_t>(m_size, PAGE_SIZE); }

		// Returns error if no memory was available
		// Returns true if page was succesfully allocated
		// Returns false if page was already allocated
		virtual BAN::ErrorOr<bool> allocate_page_containing(vaddr_t address) = 0;

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) = 0;
	
	protected:
		MemoryRegion(PageTable&, size_t size, Type type, PageTable::flags_t flags);
		BAN::ErrorOr<void> initialize(AddressRange);

	protected:
		PageTable& m_page_table;
		const size_t m_size;
		const Type m_type;
		const PageTable::flags_t m_flags;
		vaddr_t m_vaddr { 0 };
	};

}