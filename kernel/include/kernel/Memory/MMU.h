#pragma once

#include <stddef.h>
#include <stdint.h>

class MMU
{
public:
	enum Flags : uint8_t 
	{
		Present = 1,
		ReadWrite = 2,
		UserSupervisor = 4,
	};

	using vaddr_t = uintptr_t;
	using paddr_t = uintptr_t;

public:
	static void intialize();
	static MMU& get();

	MMU();
	~MMU();

	void map_page(uintptr_t, uint8_t);
	void map_range(uintptr_t, ptrdiff_t, uint8_t);

	void unmap_page(uintptr_t);
	void unmap_range(uintptr_t, ptrdiff_t);

	void map_page_at(paddr_t, vaddr_t, uint8_t);

private:
	uint64_t* m_highest_paging_struct;
};
