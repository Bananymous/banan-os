#pragma once

#include <stddef.h>
#include <stdint.h>

class MMU
{
public:
	static void Intialize();
	static MMU& Get();

	void AllocatePage(uintptr_t);
	void AllocateRange(uintptr_t, ptrdiff_t);

	void UnAllocatePage(uintptr_t);
	void UnAllocateRange(uintptr_t, ptrdiff_t);

private:
	MMU();

private:
	uint64_t* m_highest_paging_struct;
};
