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

		virtual bool pre_scheduler_sleep_needs_lock() const = 0;
		virtual void pre_scheduler_sleep_ns(uint64_t) = 0;

	protected:
		bool should_invoke_scheduler() const { return m_should_invoke_scheduler; }

	private:
		bool m_should_invoke_scheduler { true };
		friend class SystemTimer;
	};

	class SystemTimer : public Timer
	{
	public:
		static void initialize(bool force_pic);
		static SystemTimer& get();
		static bool is_initialized();

		void initialize_tsc();

		virtual uint64_t ms_since_boot() const override;
		virtual uint64_t ns_since_boot() const override;
		virtual timespec time_since_boot() const override;

		virtual bool pre_scheduler_sleep_needs_lock() const override;
		virtual void pre_scheduler_sleep_ns(uint64_t) override;

		void sleep_ms(uint64_t ms) const { ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(ms, 1'000'000)); return sleep_ns(ms * 1'000'000); }
		void sleep_ns(uint64_t ns) const;

		void dont_invoke_scheduler() { m_timer->m_should_invoke_scheduler = false; }

		void update_tsc() const;
		uint64_t ns_since_boot_no_tsc() const;

		timespec real_time() const;

	private:
		SystemTimer() = default;

		void initialize_timers(bool force_pic);

		uint64_t get_tsc_frequency() const;

	private:
		uint64_t m_boot_time { 0 };
		BAN::UniqPtr<RTC> m_rtc;
		BAN::UniqPtr<Timer> m_timer;
		bool m_has_invariant_tsc { false };
		mutable uint32_t m_timer_ticks { 0 };
	};

}
