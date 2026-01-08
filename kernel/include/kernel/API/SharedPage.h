#pragma once

#include <stdint.h>

namespace Kernel::API
{

	enum SharedPageFeature : uint32_t
	{
		SPF_GETTIME = 1 << 0,
	};

	struct SharedPage
	{
		uint8_t __sequence[0x100];

		uint32_t features;

		struct
		{
			uint8_t shift;
			uint64_t mult;
			uint64_t realtime_seconds;
		} gettime_shared;

		struct
		{
			struct
			{
				uint32_t seq;
				uint64_t last_ns;
				uint64_t last_tsc;
			} gettime_local;
		} cpus[];
	};

}
