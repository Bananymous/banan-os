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

public:
	static void intialize();
	static MMU& get();

	MMU();
	~MMU();

	void allocate_page(uintptr_t, uint8_t);
	void allocate_range(uintptr_t, ptrdiff_t, uint8_t);

	void unallocate_page(uintptr_t);
	void unallocate_range(uintptr_t, ptrdiff_t);

private:
	uint64_t* m_highest_paging_struct;
};
