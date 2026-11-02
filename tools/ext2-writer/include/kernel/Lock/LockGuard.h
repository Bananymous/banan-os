#pragma once

namespace Kernel
{

	template<typename Lock>
	class LockGuard
	{
	public:
		LockGuard(Lock&) {}
	};

}
