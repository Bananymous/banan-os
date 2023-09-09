#pragma once

#include <BAN/Time.h>

namespace Kernel
{

	class RTC
	{
	public:
		BAN::Time get_current_time();

	private:
		bool is_update_in_progress();
		uint8_t read_register8(uint8_t reg);
		void get_time(BAN::Time& out);
	};

}
