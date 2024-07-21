#pragma once

namespace Kernel
{

	class ThreadBlocker
	{
	public:
		void block_indefinite();
		void block_with_timeout_ms(uint64_t timeout_ms) { return block_with_timeout_ns(timeout_ms * 1'000'000); }
		void block_with_wake_time_ms(uint64_t wake_time_ms) { return block_with_wake_time_ns(wake_time_ms * 1'000'000); }
		void block_with_timeout_ns(uint64_t timeout_ns);
		void block_with_wake_time_ns(uint64_t wake_time_ns);
		void unblock();
	};

}
