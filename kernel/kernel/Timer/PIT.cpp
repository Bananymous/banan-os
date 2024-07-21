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

#define ACCESS_HI 			0x10
#define ACCESS_LO 			0x20

#define MODE_SQUARE_WAVE	0x06

#define BASE_FREQUENCY		1193182
#define TICKS_PER_SECOND	1000

#define MS_PER_S			1'000
#define NS_PER_S			1'000'000'000

namespace Kernel
{

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
		constexpr uint16_t timer_reload = BASE_FREQUENCY / TICKS_PER_SECOND;

		IO::outb(PIT_CTL, SELECT_CHANNEL0 | ACCESS_LO | ACCESS_HI | MODE_SQUARE_WAVE);

		IO::outb(TIMER0_CTL, (timer_reload >> 0) & 0xff);
		IO::outb(TIMER0_CTL, (timer_reload >> 8) & 0xff);

		MUST(InterruptController::get().reserve_irq(PIT_IRQ));
		set_irq(PIT_IRQ);
		enable_interrupt();
	}

	void PIT::handle_irq()
	{
		{
			SpinLockGuard _(m_lock);
			m_system_time++;
		}
		Processor::scheduler().timer_interrupt();
	}

	uint64_t PIT::read_counter() const
	{
		SpinLockGuard _(m_lock);
		return m_system_time;
	}

	uint64_t PIT::ms_since_boot() const
	{
		return read_counter() * (MS_PER_S / TICKS_PER_SECOND);
	}

	uint64_t PIT::ns_since_boot() const
	{
		return read_counter() * (NS_PER_S / TICKS_PER_SECOND);
	}

	timespec PIT::time_since_boot() const
	{
		uint64_t ticks = read_counter();
		return timespec {
			.tv_sec = ticks / TICKS_PER_SECOND,
			.tv_nsec = (long)((ticks % TICKS_PER_SECOND) * (NS_PER_S / TICKS_PER_SECOND))
		};
	}

}
