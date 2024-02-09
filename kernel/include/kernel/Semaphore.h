#pragma once

namespace Kernel
{

	class Semaphore
	{
	public:
		void block_indefinite();
		void block_with_timeout(uint64_t timeout_ms);
		void block_with_wake_time(uint64_t wake_time_ms);
		void unblock();
	};

}
