#pragma once

namespace Kernel
{

	struct termios
	{
		bool canonical { true };
		bool echo { true };
	};

}
