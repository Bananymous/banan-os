#include <kernel/Timer/Timer.h>
#include <kernel/Timer/PIT.h>

namespace Kernel
{

	static TimerHandler* s_instance = nullptr;

	void TimerHandler::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new TimerHandler;
		ASSERT(s_instance);
		s_instance->initialize_timers();
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
		if (auto res = PIT::create(); res.is_error())
			dwarnln("PIT: {}", res.error());
		else
		{
			MUST(m_timers.emplace_back(BAN::move(res.release_value())));
			dprintln("PIT initialized");
		}

		ASSERT(!m_timers.empty());
	}

	uint64_t TimerHandler::ms_since_boot() const
	{
		return m_timers.front()->ms_since_boot();
	}

	void TimerHandler::sleep(uint64_t ms) const
	{
		return m_timers.front()->sleep(ms);
	}

}