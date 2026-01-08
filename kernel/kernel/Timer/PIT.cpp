#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Processor.h>
#include <kernel/Timer/PIT.h>

#define PIT_IRQ 0

#define TIMER0_CTL			0x40
#define TIMER1_CTL			0x41
#define TIMER2_CTL			0x42
#define PIT_CTL				0x43

#define SELECT_CHANNEL0		0x00
#define SELECT_CHANNEL1		0x40
#define SELECT_CHANNEL2		0x80

#define ACCESS_LO 			0x10
#define ACCESS_HI 			0x20

#define MODE_RATE_GENERATOR	0x04

#define BASE_FREQUENCY		1193182

#define MS_PER_S			1'000
#define NS_PER_MS			1'000'000
#define NS_PER_S			1'000'000'000

namespace Kernel
{

	constexpr uint16_t s_ticks_per_ms = BASE_FREQUENCY / 1000;

	BAN::ErrorOr<BAN::UniqPtr<PIT>> PIT::create()
	{
		PIT* pit = new PIT();
		if (pit == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		pit->initialize();
		return BAN::UniqPtr<PIT>::adopt(pit);
	}

	void PIT::initialize()
	{
		IO::outb(PIT_CTL, SELECT_CHANNEL0 | ACCESS_LO | ACCESS_HI | MODE_RATE_GENERATOR);
		IO::outb(TIMER0_CTL, (s_ticks_per_ms >> 0) & 0xff);
		IO::outb(TIMER0_CTL, (s_ticks_per_ms >> 8) & 0xff);

		MUST(InterruptController::get().reserve_irq(PIT_IRQ));
		set_irq(PIT_IRQ);
		InterruptController::get().enable_irq(PIT_IRQ);
	}

	void PIT::handle_irq()
	{
		{
			SpinLockGuard _(m_lock);
			m_system_time_ms++;
		}

		SystemTimer::get().update_tsc();

		if (should_invoke_scheduler())
			Processor::scheduler().timer_interrupt();
	}

	uint64_t PIT::read_counter_ns() const
	{
		SpinLockGuard _(m_lock);

		// send latch command
		IO::outb(PIT_CTL, SELECT_CHANNEL0);

		// read ticks
		uint64_t ticks_this_ms { 0 };
		ticks_this_ms |= IO::inb(TIMER0_CTL);
		ticks_this_ms |= IO::inb(TIMER0_CTL) << 8;
		ticks_this_ms = s_ticks_per_ms - ticks_this_ms;

		const uint64_t ns_this_ms = ticks_this_ms * NS_PER_S / BASE_FREQUENCY;
		return (m_system_time_ms * NS_PER_MS) + ns_this_ms;
	}

	uint64_t PIT::ms_since_boot() const
	{
		return read_counter_ns() / NS_PER_MS;
	}

	uint64_t PIT::ns_since_boot() const
	{
		return read_counter_ns();
	}

	timespec PIT::time_since_boot() const
	{
		uint64_t ns = read_counter_ns();
		return timespec {
			.tv_sec = static_cast<time_t>(ns / NS_PER_S),
			.tv_nsec = static_cast<long>(ns % NS_PER_S)
		};
	}

	void PIT::pre_scheduler_sleep_ns(uint64_t ns)
	{
		const uint64_t target_ticks = BAN::Math::div_round_up<uint64_t>(ns * BASE_FREQUENCY, NS_PER_S);

		SpinLockGuard _(m_lock);

		IO::outb(PIT_CTL, SELECT_CHANNEL0 | ACCESS_LO | ACCESS_HI | MODE_RATE_GENERATOR);
		IO::outb(TIMER0_CTL, 0);
		IO::outb(TIMER0_CTL, 0);

		IO::outb(PIT_CTL, SELECT_CHANNEL0 | ACCESS_LO | MODE_RATE_GENERATOR);
		IO::outb(TIMER0_CTL, 0xFF);

		uint64_t elapsed_ticks = 0;
		uint8_t last_ticks = IO::inb(TIMER0_CTL);
		while (elapsed_ticks < target_ticks)
		{
			const uint8_t current_ticks = IO::inb(TIMER0_CTL);
			elapsed_ticks += static_cast<uint8_t>(last_ticks - current_ticks);
			last_ticks = current_ticks;
		}

		IO::outb(PIT_CTL, SELECT_CHANNEL0 | ACCESS_LO | ACCESS_HI | MODE_RATE_GENERATOR);
		IO::outb(TIMER0_CTL, (s_ticks_per_ms >> 0) & 0xFF);
		IO::outb(TIMER0_CTL, (s_ticks_per_ms >> 8) & 0xFF);
	}

}
