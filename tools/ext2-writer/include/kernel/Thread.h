#pragma once

#include <BAN/Errors.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class Thread
	{
	public:
		static Thread& current()
		{
			static Thread s_instance;
			return s_instance;
		}

		static pid_t current_tid()
		{
			return 1;
		}

		BAN::ErrorOr<void> block_or_eintr_indefinite(ThreadBlocker&, Mutex*)
		{
			return {};
		}
	};

}
