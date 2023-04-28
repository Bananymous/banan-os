#pragma once

#include <kernel/Memory/Heap.h>

namespace Kernel
{
	
	class MMU
	{
	public:
		enum Flags : uint8_t 
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

		void identity_map_page(paddr_t, uint8_t);
		void identity_map_range(paddr_t, ptrdiff_t, uint8_t);

		void unmap_page(vaddr_t);
		void unmap_range(vaddr_t, ptrdiff_t);

		void map_page_at(paddr_t, vaddr_t, uint8_t);

		uint8_t get_page_flags(vaddr_t) const;

		void load();

	private:
		void initialize_kernel();

	private:
		uint64_t* m_highest_paging_struct;
	};

}
