#pragma once

#include <stdint.h>

namespace LibInput
{

	// TODO: not used but here if we ever make controller
	//       support generating events instead of being polled
	struct JoystickEvent
	{

	};

	struct JoystickState
	{
		struct Axis
		{
			int64_t value;
			int64_t min;
			int64_t max;
		};

		Axis axis[4];
		bool buttons[32];
	};

}
