#pragma once

#include <stdint.h>

namespace Paging
{

	void Initialize();

	void MapFramebuffer(uint32_t address);
	void MapRSDP(uint32_t address);
	void MapAPIC(uint32_t address);

}