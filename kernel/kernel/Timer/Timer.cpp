#include <kernel/Scheduler.h>
#include <kernel/Timer/HPET.h>
#include <kernel/Timer/PIT.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static TimerHandler* s_instance = nullptr;

	void TimerHandler::initialize()
	{
		ASSERT(s_instance == nullptr);
		auto* temp = new TimerHandler;
		ASSERT(temp);
		temp->initialize_timers();
		s_instance = temp;
	}

	TimerHandler& TimerHandler::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	bool TimerHandler::is_initialized()
	{
		return !!s_instance;
	}

	void TimerHandler::initialize_timers()
	{
		m_rtc = MUST(BAN::UniqPtr<RTC>::create());
		m_boot_time = BAN::to_unix_time(m_rtc->get_current_time());

		if (auto res = HPET::create(); res.is_error())
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

	uint64_t TimerHandler::ms_since_boot() const
	{
		return m_timer->ms_since_boot();
	}

	timespec TimerHandler::time_since_boot() const
	{
		return m_timer->time_since_boot();
	}

	void TimerHandler::sleep(uint64_t ms) const
	{
		if (ms == 0)
			return;
		uint64_t wake_time = ms_since_boot() + ms;
		Scheduler::get().set_current_thread_sleeping(wake_time);
		if (ms_since_boot() < wake_time)
			dwarnln("sleep woke {} ms too soon", wake_time - ms_since_boot());
	}

	uint64_t TimerHandler::get_unix_timestamp()
	{
		return m_boot_time + ms_since_boot() / 1000;
	}

}