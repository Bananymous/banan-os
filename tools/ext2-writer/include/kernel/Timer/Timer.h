#pragma once

#include <time.h>

namespace Kernel
{

	class SystemTimer
	{
	public:
		static SystemTimer& get()
		{
			static SystemTimer s_instance;
			return s_instance;
		}

		timespec real_time()
		{
			timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			return ts;
		}

	private:
		SystemTimer() = default;
	};

}
