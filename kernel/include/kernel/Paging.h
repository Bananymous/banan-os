#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Paging
{

	void Initialize();

	void MapPage(uintptr_t address);
	void MapPages(uintptr_t address, size_t size);

}