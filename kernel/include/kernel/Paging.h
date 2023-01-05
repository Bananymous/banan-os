#pragma once

#include <stdint.h>

namespace Paging
{

	void Initialize();

	void MapPage(uintptr_t address);

}