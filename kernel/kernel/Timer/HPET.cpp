#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Processor.h>
#include <kernel/Timer/HPET.h>

#define HPET_PERIOD_MAX 0x05F5E100

#define FS_PER_S	1'000'000'000'000'000
#define FS_PER_MS	1'000'000'000'000
#define FS_PER_US	1'000'000'000
#define FS_PER_NS	1'000'000

namespace Kernel
{

	enum HPETCapabilities : uint32_t
	{
		LEG_RT_CAP		= 1 << 16,
		COUNT_SIZE_CAP	= 1 << 13,

		NUM_TIM_CAP_MASK	= 0x1F << 8,
		NUM_TIM_CAP_SHIFT	= 8,
	};

	enum HPETConfiguration : uint32_t
	{
		LEG_RT_CNF	= 1 << 1,
		ENABLE_CNF	= 1 << 0,
	};

	enum HPETTimerConfiguration : uint32_t
	{
		Tn_INT_TYPE_CNF		= 1 << 1,
		Tn_INT_ENB_CNF		= 1 << 2,
		Tn_TYPE_CNF			= 1 << 3,
		Tn_PER_INT_CAP		= 1 << 4,
		Tn_SIZE_CAP			= 1 << 5,
		Tn_VAL_SET_CNF		= 1 << 6,
		Tn_32MODE_CNF		= 1 << 8,
		Tn_FSB_EN_CNF		= 1 << 14,
		Tn_FSB_INT_DEL_CAP	= 1 << 14,

		Tn_INT_ROUTE_CNF_MASK	= 0x1F << 9,
		Tn_INT_ROUTE_CNF_SHIFT	= 9,
	};

	struct HPETRegister
	{
		union
		{
			uint64_t full;
			struct
			{
				uint32_t low;
				uint32_t high;
			};
		};
	};
	static_assert(sizeof(HPETRegister) == 8);

	struct HPETTimer
	{
		uint32_t		configuration;
		uint32_t		int_route_cap;
		HPETRegister	comparator;
		HPETRegister	fsb_interrupt_route;
		uint64_t		__reserved;
	};
	static_assert(sizeof(HPETTimer) == 32);

	struct HPETRegisters
	{
		/*
			63:32	COUNTER_CLK_PERIOD
			31:16	VENDOR_ID
			15		LEG_RT_CAP
			13		COUNT_SIZE_CAP
			12:8	NUM_TIM_CAP
			7:0		REV_ID
		*/
		uint32_t		capabilities;
		uint32_t		counter_clk_period;

		uint64_t 		__reserved0;

		/*
			1	LEG_RT_CNF
			0	ENABLE_CNF
		*/
		HPETRegister	configuration;

		uint64_t		__reserved1;

		/*
			N	Tn_INT_STS
		*/
		HPETRegister	interrupt_status;

		uint8_t			__reserved2[0xF0 - 0x28];

		HPETRegister	main_counter;

		uint64_t		__reserved3;

		HPETTimer		timers[32];
	};
	static_assert(offsetof(HPETRegisters, main_counter)	== 0xF0);
	static_assert(offsetof(HPETRegisters, timers[0])	== 0x100);
	static_assert(offsetof(HPETRegisters, timers[1])	== 0x120);

	BAN::ErrorOr<BAN::UniqPtr<HPET>> HPET::create(bool force_pic)
	{
		HPET* hpet_ptr = new HPET();
		if (hpet_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto hpet = BAN::UniqPtr<HPET>::adopt(hpet_ptr);
		TRY(hpet->initialize(force_pic));
		return hpet;
	}

	HPET::~HPET()
	{
		if (m_mmio_base)
			PageTable::kernel().unmap_page(m_mmio_base);
		m_mmio_base = 0;
	}

	BAN::ErrorOr<void> HPET::initialize(bool force_pic)
	{
		auto* header = static_cast<const ACPI::HPET*>(ACPI::ACPI::get().get_header("HPET"_sv, 0));
		if (header == nullptr)
			return BAN::Error::from_errno(ENODEV);

		if (header->hardware_rev_id == 0)
			return BAN::Error::from_errno(EINVAL);

		if (force_pic && !header->legacy_replacement_irq_routing_cable)
		{
			dwarnln("HPET doesn't support legacy mapping");
			return BAN::Error::from_errno(ENOTSUP);
		}

		m_mmio_base = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
		ASSERT(m_mmio_base);

		PageTable::kernel().map_page_at(header->base_address.address, m_mmio_base, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

		auto& regs = registers();

		m_is_64bit = regs.capabilities & COUNT_SIZE_CAP;

		// Disable and reset main counter
		regs.configuration.low = regs.configuration.low & ~ENABLE_CNF;
		regs.main_counter.high = 0;
		regs.main_counter.low = 0;

		// Enable legacy routing if available
		if (regs.capabilities & LEG_RT_CAP)
			regs.configuration.low = regs.configuration.low | LEG_RT_CNF;

		uint32_t period_fs = regs.counter_clk_period;
		if (period_fs == 0 || period_fs > HPET_PERIOD_MAX)
		{
			dwarnln("HPET: Invalid counter period");
			return BAN::Error::from_errno(EINVAL);
		}

		m_ticks_per_s = FS_PER_S / period_fs;
		dprintln("HPET frequency {} Hz", m_ticks_per_s);

		uint8_t last_timer = (regs.capabilities & NUM_TIM_CAP_MASK) >> NUM_TIM_CAP_SHIFT;
		dprintln("HPET has {} timers", last_timer + 1);

		// Disable all timers
		for (uint8_t i = 0; i <= last_timer; i++)
		{
			auto& timer_regs = regs.timers[i];
			timer_regs.configuration = timer_regs.configuration & ~Tn_INT_ENB_CNF;
		}

		auto& timer0 = regs.timers[0];
		if (!(timer0.configuration & Tn_PER_INT_CAP))
		{
			dwarnln("HPET: timer0 cannot be periodic");
			return BAN::Error::from_errno(ENOTSUP);
		}

		// enable interrupts
		timer0.configuration  = timer0.configuration | Tn_INT_ENB_CNF;
		// clear interrupt mask (set irq to 0)
		timer0.configuration = timer0.configuration & ~Tn_INT_ROUTE_CNF_MASK;
		// edge triggered interrupts
		timer0.configuration = timer0.configuration & ~Tn_INT_TYPE_CNF;
		// periodic timer
		timer0.configuration = timer0.configuration | Tn_TYPE_CNF;
		// disable 32 bit mode
		timer0.configuration = timer0.configuration & ~Tn_32MODE_CNF;
		// disable FSB interrupts
		if (timer0.configuration & Tn_FSB_INT_DEL_CAP)
			timer0.configuration = timer0.configuration & ~Tn_FSB_EN_CNF;

		// set timer period to 1000 Hz
		uint64_t ticks_per_ms = m_ticks_per_s / 1000;
		timer0.configuration = timer0.configuration | Tn_VAL_SET_CNF;
		timer0.comparator.low = ticks_per_ms;
		if (timer0.configuration & Tn_SIZE_CAP)
		{
			timer0.configuration = timer0.configuration | Tn_VAL_SET_CNF;
			timer0.comparator.high = ticks_per_ms >> 32;
		}
		else if (ticks_per_ms > 0xFFFFFFFF)
		{
			dprintln("HPET: cannot create 1 kHz timer");
			return BAN::Error::from_errno(ENOTSUP);
		}

		// enable main counter
		regs.configuration.low = regs.configuration.low | ENABLE_CNF;

		TRY(InterruptController::get().reserve_irq(0));
		set_irq(0);
		InterruptController::get().enable_irq(0);

		return {};
	}

	volatile HPETRegisters& HPET::registers()
	{
		return *reinterpret_cast<volatile HPETRegisters*>(m_mmio_base);
	}

	const volatile HPETRegisters& HPET::registers() const
	{
		return *reinterpret_cast<const volatile HPETRegisters*>(m_mmio_base);
	}

	uint64_t HPET::read_main_counter() const
	{
		auto& regs = registers();
		if (m_is_64bit)
			return regs.main_counter.full;

		SpinLockGuard _(m_lock);
		uint32_t current_low = regs.main_counter.low;
		uint32_t wraps = m_32bit_wraps;
		if (current_low < (uint32_t)m_last_ticks)
			wraps++;
		return ((uint64_t)wraps << 32) | current_low;
	}

	void HPET::handle_irq()
	{
		{
			auto& regs = registers();

			SpinLockGuard _(m_lock);

			uint64_t current_ticks;
			if (m_is_64bit)
				current_ticks = regs.main_counter.full;
			else
			{
				uint32_t current_low = regs.main_counter.low;
				if (current_low < (uint32_t)m_last_ticks)
					m_32bit_wraps++;
				current_ticks = ((uint64_t)m_32bit_wraps << 32) | current_low;
			}
			m_last_ticks = current_ticks;
		}

		if (should_invoke_scheduler())
			Processor::scheduler().timer_interrupt();
	}

	uint64_t HPET::ms_since_boot() const
	{
		auto current = time_since_boot();
		return current.tv_sec * 1'000 + current.tv_nsec / 1'000'000;
	}

	uint64_t HPET::ns_since_boot() const
	{
		auto current = time_since_boot();
		return current.tv_sec * 1'000'000'000 + current.tv_nsec;
	}

	timespec HPET::time_since_boot() const
	{
		auto& regs = registers();

		uint64_t counter = read_main_counter();
		uint64_t seconds			= counter / m_ticks_per_s;
		uint64_t ticks_this_second	= counter % m_ticks_per_s;

		long ns_this_second = ticks_this_second * regs.counter_clk_period / FS_PER_NS;

		return timespec {
			.tv_sec = seconds,
			.tv_nsec = ns_this_second
		};
	}

	void HPET::pre_scheduler_sleep_ns(uint64_t ns)
	{
		auto& regs = registers();

		const uint64_t target_ticks = BAN::Math::div_round_up<uint64_t>(ns * FS_PER_NS, regs.counter_clk_period);

		if (m_is_64bit)
		{
			const uint64_t target_counter = regs.main_counter.full + target_ticks;
			while (regs.main_counter.full < target_counter)
				__builtin_ia32_pause();
		}
		else
		{
			uint64_t elapsed_ticks = 0;
			uint64_t last_counter = regs.main_counter.low;
			while (elapsed_ticks < target_ticks)
			{
				const uint64_t current_counter = regs.main_counter.low;
				if (last_counter <= current_counter)
					elapsed_ticks += current_counter - last_counter;
				else
					elapsed_ticks += 0xFFFFFFFF + current_counter - last_counter;
				last_counter = current_counter;
				__builtin_ia32_pause();
			}
		}
	}

}
