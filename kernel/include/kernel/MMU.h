#pragma once

#include <stddef.h>
#include <stdint.h>

class MMU
{
public:
	static void intialize();
	static MMU& get();

	MMU();
	~MMU();

	void allocate_page(uintptr_t);
	void allocate_range(uintptr_t, ptrdiff_t);

	void unallocate_page(uintptr_t);
	void unallocate_range(uintptr_t, ptrdiff_t);

private:
	uint64_t* m_highest_paging_struct;
};
