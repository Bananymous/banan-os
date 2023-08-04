#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Timer/RTC.h>

namespace Kernel
{

	class Timer
	{
	public:
		virtual ~Timer() {};
		virtual uint64_t ms_since_boot() const = 0;
		virtual timespec time_since_boot() const = 0;
	};

	class SystemTimer : public Timer
	{
	public:
		static void initialize();
		static SystemTimer& get();
		static bool is_initialized();

		virtual uint64_t ms_since_boot() const override;
		virtual timespec time_since_boot() const override;
		
		void sleep(uint64_t) const;

		uint64_t get_unix_timestamp() const;
		timespec get_real_time() const;

	private:
		SystemTimer() = default;

		void initialize_timers();

	private:
		uint64_t m_boot_time { 0 };
		BAN::UniqPtr<RTC> m_rtc;
		BAN::UniqPtr<Timer> m_timer;
	};

}