#pragma once

#include <kernel/Memory/Heap.h>

namespace Kernel
{

	class MMU
	{
	public:
		using flags_t = uint8_t;
		enum Flags : flags_t 
		{
			Present = 1,
			ReadWrite = 2,
			UserSupervisor = 4,
		};

	public:
		static void initialize();
		static MMU& get();

		MMU();
		~MMU();

		void identity_map_page(paddr_t, flags_t);
		void identity_map_range(paddr_t, size_t, flags_t);

		void unmap_page(vaddr_t);
		void unmap_range(vaddr_t, size_t);

		void map_page_at(paddr_t, vaddr_t, flags_t);

		flags_t get_page_flags(vaddr_t) const;

		vaddr_t get_free_page() const;
		vaddr_t get_free_contiguous_pages(uint32_t) const;

		void load();

	private:
		void initialize_kernel();

	private:
		uint64_t* m_highest_paging_struct;
	};

}
