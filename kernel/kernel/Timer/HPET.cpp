#include <BAN/ScopeGuard.h>
#include <kernel/ACPI.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/HPET.h>

#define HPET_REG_CAPABILIES	0x00
#define HPET_REG_CONFIG		0x10
#define HPET_REG_COUNTER	0xF0

#define HPET_CONFIG_ENABLE	0x01
#define HPET_CONFIG_LEG_RT	0x02

#define HPET_REG_TIMER_CONFIG(N)		(0x100 + 0x20 * N)
#define HPET_REG_TIMER_COMPARATOR(N)	(0x108 + 0x20 * N)

#define HPET_Tn_INT_ENB_CNF	(1 << 2)
#define HPET_Tn_TYPE_CNF	(1 << 3)
#define HPET_Tn_PER_INT_CAP	(1 << 4)
#define HPET_Tn_VAL_SET_CNF	(1 << 6)
#define HPET_Tn_INT_ROUTE_CNF_SHIFT 9

#define FS_PER_S	1'000'000'000'000'000
#define FS_PER_MS	1'000'000'000'000
#define FS_PER_US	1'000'000'000
#define FS_PER_NS	1'000'000

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<HPET>> HPET::create(bool force_pic)
	{
		HPET* hpet = new HPET();
		if (hpet == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		if (auto ret = hpet->initialize(force_pic); ret.is_error())
		{
			delete hpet;
			return ret.release_error();
		}
		return BAN::UniqPtr<HPET>::adopt(hpet);
	}

	BAN::ErrorOr<void> HPET::initialize(bool force_pic)
	{
		auto* header = (ACPI::HPET*)ACPI::get().get_header("HPET"sv, 0);
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
		BAN::ScopeGuard unmapper([this] { PageTable::kernel().unmap_page(m_mmio_base); });

		m_counter_tick_period_fs = read_register(HPET_REG_CAPABILIES) >> 32;

		// period has to be less than 100 ns
		if (m_counter_tick_period_fs == 0 || m_counter_tick_period_fs > FS_PER_NS * 100)
			return BAN::Error::from_errno(EINVAL);

		uint64_t ticks_per_ms = FS_PER_MS / m_counter_tick_period_fs;
		
		{
			const char* units[] = { "fs", "ps", "ns" };
			int index = 0;
			uint64_t temp = m_counter_tick_period_fs;
			while (temp >= 1000)
			{
				temp /= 1000;
				index++;
			}
			dprintln("HPET percision {} {}", temp, units[index]);
		}

		uint64_t timer0_config = read_register(HPET_REG_TIMER_CONFIG(0));
		if (!(timer0_config & HPET_Tn_PER_INT_CAP))
		{
			dwarnln("timer 0 doesn't support periodic");
			return BAN::Error::from_errno(ENOTSUP);
		}

		int irq = 0;
		if (!force_pic)
		{
			uint32_t irq_cap = timer0_config >> 32;
			if (irq_cap == 0)
			{
				dwarnln("HPET doesn't have any interrupts available");
				return BAN::Error::from_errno(EINVAL);
			}
			for (irq = 0; irq < 32; irq++)
				if (irq_cap & (1 << irq))
					break;
		}

		unmapper.disable();

		uint64_t main_flags = HPET_CONFIG_ENABLE;
		if (force_pic)
			main_flags |= HPET_CONFIG_LEG_RT;

		// Enable main counter
		write_register(HPET_REG_CONFIG, read_register(HPET_REG_CONFIG) | main_flags);

		uint64_t timer0_flags = 0;
		timer0_flags |= HPET_Tn_INT_ENB_CNF;
		timer0_flags |= HPET_Tn_TYPE_CNF;
		timer0_flags |= HPET_Tn_VAL_SET_CNF;
		if (!force_pic)
			timer0_flags |= irq << HPET_Tn_INT_ROUTE_CNF_SHIFT;

		// Enable timer 0 as 1 ms periodic
		write_register(HPET_REG_TIMER_CONFIG(0), timer0_flags);
		write_register(HPET_REG_TIMER_COMPARATOR(0), read_register(HPET_REG_COUNTER) + ticks_per_ms);
		write_register(HPET_REG_TIMER_COMPARATOR(0), ticks_per_ms);

		// Disable timers 1->
		for (int i = 1; i <= header->comparator_count; i++)
			write_register(HPET_REG_TIMER_CONFIG(i), 0);

		set_irq(irq);
		enable_interrupt();

		return {};
	}

	void HPET::handle_irq()
	{
		Scheduler::get().timer_reschedule();
	}

	uint64_t HPET::ms_since_boot() const
	{
		// FIXME: 32 bit CPUs should use 32 bit counter with 32 bit reads
		return read_register(HPET_REG_COUNTER) * m_counter_tick_period_fs / FS_PER_MS;
	}

	uint64_t HPET::ns_since_boot() const
	{
		// FIXME: 32 bit CPUs should use 32 bit counter with 32 bit reads
		return read_register(HPET_REG_COUNTER) * m_counter_tick_period_fs / FS_PER_NS;
	}

	timespec HPET::time_since_boot() const
	{
		uint64_t time_fs = read_register(HPET_REG_COUNTER) * m_counter_tick_period_fs;
		return timespec {
			.tv_sec = time_fs / FS_PER_S,
			.tv_nsec = (long)((time_fs % FS_PER_S) / FS_PER_NS)
		};
	}

	void HPET::write_register(ptrdiff_t reg, uint64_t value) const
	{
		MMIO::write64(m_mmio_base + reg, value);
	}

	uint64_t HPET::read_register(ptrdiff_t reg) const
	{
		return MMIO::read64(m_mmio_base + reg);
	}

}