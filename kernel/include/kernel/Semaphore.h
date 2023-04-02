#pragma once

namespace Kernel
{

	class Semaphore
	{
	public:
		void block();
		void unblock();
		bool is_blocked() const { return m_blocked; }

	private:
		void set_blocked(bool blocked) { m_blocked = blocked; }

	private:
		bool m_blocked { false };

		friend class Scheduler;
	};

}