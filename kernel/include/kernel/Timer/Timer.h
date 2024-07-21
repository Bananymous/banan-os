#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Timer/RTC.h>

#include <time.h>

namespace Kernel
{

	class Timer
	{
	public:
		virtual ~Timer() {};
		virtual uint64_t ms_since_boot() const = 0;
		virtual uint64_t ns_since_boot() const = 0;
		virtual timespec time_since_boot() const = 0;
	};

	class SystemTimer : public Timer
	{
	public:
		static void initialize(bool force_pic);
		static SystemTimer& get();
		static bool is_initialized();

		virtual uint64_t ms_since_boot() const override;
		virtual uint64_t ns_since_boot() const override;
		virtual timespec time_since_boot() const override;

		void sleep_ms(uint64_t ms) const { return sleep_ns(ms * 1'000'000); }
		void sleep_ns(uint64_t ns) const;

		timespec real_time() const;

	private:
		SystemTimer() = default;

		void initialize_timers(bool force_pic);

	private:
		uint64_t m_boot_time { 0 };
		BAN::UniqPtr<RTC> m_rtc;
		BAN::UniqPtr<Timer> m_timer;
	};

}
