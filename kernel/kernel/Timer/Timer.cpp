#include <kernel/Scheduler.h>
#include <kernel/Timer/HPET.h>
#include <kernel/Timer/PIT.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static SystemTimer* s_instance = nullptr;

	void SystemTimer::initialize(bool force_pic)
	{
		ASSERT(s_instance == nullptr);
		auto* temp = new SystemTimer;
		ASSERT(temp);
		temp->initialize_timers(force_pic);
		s_instance = temp;
	}

	SystemTimer& SystemTimer::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	bool SystemTimer::is_initialized()
	{
		return !!s_instance;
	}

	void SystemTimer::initialize_timers(bool force_pic)
	{
		m_rtc = MUST(BAN::UniqPtr<RTC>::create());
		m_boot_time = BAN::to_unix_time(m_rtc->get_current_time());

		if (auto res = HPET::create(force_pic); res.is_error())
			dwarnln("HPET: {}", res.error());
		else
		{
			m_timer = res.release_value();
			dprintln("HPET initialized");
			return;
		}

		if (auto res = PIT::create(); res.is_error())
			dwarnln("PIT: {}", res.error());
		else
		{
			m_timer = res.release_value();
			dprintln("PIT initialized");
			return;
		}

		Kernel::panic("Could not initialize any timer");
	}

	uint64_t SystemTimer::ms_since_boot() const
	{
		return m_timer->ms_since_boot();
	}

	timespec SystemTimer::time_since_boot() const
	{
		return m_timer->time_since_boot();
	}

	void SystemTimer::sleep(uint64_t ms) const
	{
		if (ms == 0)
			return;
		uint64_t wake_time = ms_since_boot() + ms;
		Scheduler::get().set_current_thread_sleeping(wake_time);
		if (ms_since_boot() < wake_time)
			dwarnln("sleep woke {} ms too soon", wake_time - ms_since_boot());
	}

	timespec SystemTimer::real_time() const
	{
		auto result = time_since_boot();
		result.tv_sec += m_boot_time;
		return result;
	}

}