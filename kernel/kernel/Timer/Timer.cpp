#include <BAN/Sort.h>

#include <kernel/CPUID.h>
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

	void SystemTimer::initialize_tsc()
	{
		if (!CPUID::has_invariant_tsc())
		{
			dwarnln("CPU does not have an invariant TSC");
			return;
		}

		const uint64_t tsc_freq = get_tsc_frequency();

		dprintln("Initialized invariant TSC ({} Hz)", tsc_freq);

		const uint8_t tsc_shift = 22;
		const uint64_t tsc_mult = (static_cast<uint64_t>(1'000'000'000) << tsc_shift) / tsc_freq;
		Processor::initialize_tsc(tsc_shift, tsc_mult, m_boot_time);

		m_has_invariant_tsc = true;
	}

	uint64_t SystemTimer::get_tsc_frequency() const
	{
		// take 5x 50 ms samples and use the median value

		constexpr size_t tsc_sample_count = 5;
		constexpr size_t tsc_sample_ns = 50'000'000;

		uint64_t tsc_freq_samples[tsc_sample_count];
		for (size_t i = 0; i < tsc_sample_count; i++)
		{
			const auto start_ns = m_timer->ns_since_boot();

			const auto start_tsc = __builtin_ia32_rdtsc();
			while (m_timer->ns_since_boot() < start_ns + tsc_sample_ns)
				Processor::pause();
			const auto stop_tsc = __builtin_ia32_rdtsc();

			const auto stop_ns = m_timer->ns_since_boot();

			const auto duration_ns = stop_ns - start_ns;
			const auto count_tsc = stop_tsc - start_tsc;

			tsc_freq_samples[i] = count_tsc * 1'000'000'000 / duration_ns;
		}

		BAN::sort::sort(tsc_freq_samples, tsc_freq_samples + tsc_sample_count);

		return tsc_freq_samples[tsc_sample_count / 2];
	}

	void SystemTimer::update_tsc() const
	{
		if (!m_has_invariant_tsc)
			return;

		// only update every 100 ms
		if (++m_timer_ticks < 100)
			return;
		m_timer_ticks = 0;

		Processor::update_tsc();
		Processor::broadcast_smp_message({
			.type = Processor::SMPMessage::Type::UpdateTSC,
			.dummy = 0,
		});
	}

	uint64_t SystemTimer::ns_since_boot_no_tsc() const
	{
		return m_timer->ns_since_boot();
	}

	uint64_t SystemTimer::ms_since_boot() const
	{
		if (!m_has_invariant_tsc)
			return m_timer->ms_since_boot();
		return Processor::ns_since_boot_tsc() / 1'000'000;
	}

	uint64_t SystemTimer::ns_since_boot() const
	{
		if (!m_has_invariant_tsc)
			return m_timer->ns_since_boot();
		return Processor::ns_since_boot_tsc();
	}

	timespec SystemTimer::time_since_boot() const
	{
		if (!m_has_invariant_tsc)
			return m_timer->time_since_boot();
		const auto ns_since_boot = Processor::ns_since_boot_tsc();
		return {
			.tv_sec = static_cast<time_t>(ns_since_boot / 1'000'000'000),
			.tv_nsec = static_cast<long>(ns_since_boot % 1'000'000'000)
		};
	}

	bool SystemTimer::pre_scheduler_sleep_needs_lock() const
	{
		return m_timer->pre_scheduler_sleep_needs_lock();
	}

	void SystemTimer::pre_scheduler_sleep_ns(uint64_t ns)
	{
		return m_timer->pre_scheduler_sleep_ns(ns);
	}

	void SystemTimer::sleep_ns(uint64_t ns) const
	{
		if (ns == 0)
			return;
		Processor::scheduler().block_current_thread(nullptr, ns_since_boot() + ns, nullptr);
	}

	timespec SystemTimer::real_time() const
	{
		auto result = time_since_boot();
		result.tv_sec += m_boot_time;
		return result;
	}

}
