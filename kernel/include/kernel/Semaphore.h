#pragma once

namespace Kernel
{

	class Semaphore
	{
	public:
		void block();
		void unblock();
	};

}