#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

namespace Kernel
{

	class Timer
	{
	public:
		virtual ~Timer() {};
		virtual uint64_t ms_since_boot() const = 0;
		virtual void sleep(uint64_t) const = 0;
	};

	class TimerHandler
	{
	public:
		static void initialize();
		static TimerHandler& get();
		static bool is_initialized();

		uint64_t ms_since_boot() const;
		void sleep(uint64_t) const;

	private:
		TimerHandler() = default;

		void initialize_timers();

	private:
		BAN::Vector<BAN::UniqPtr<Timer>> m_timers;
	};

}