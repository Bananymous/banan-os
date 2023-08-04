#pragma once

#include <kernel/Timer/Timer.h>

namespace Kernel
{

	class HPET final : public Timer
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<HPET>> create();

		virtual uint64_t ms_since_boot() const override;
		virtual timespec time_since_boot() const override;

	private:
		HPET() = default;
		BAN::ErrorOr<void> initialize();

		void write_register(ptrdiff_t reg, uint64_t value) const;
		uint64_t read_register(ptrdiff_t reg) const;

	private:
		uint64_t m_counter_tick_period_fs { 0 };
		vaddr_t m_mmio_base { 0 };
	};

}