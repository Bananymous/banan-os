#pragma once

#include <kernel/Interruptable.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	struct HPETRegisters;

	class HPET final : public Timer, public Interruptable
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<HPET>> create(bool force_pic);
		~HPET();

		virtual uint64_t ms_since_boot() const override;
		virtual uint64_t ns_since_boot() const override;
		virtual timespec time_since_boot() const override;

		virtual void handle_irq() override;

	private:
		HPET() = default;
		BAN::ErrorOr<void> initialize(bool force_pic);

		volatile HPETRegisters& registers();
		const volatile HPETRegisters& registers() const;

		uint64_t read_main_counter() const;

	private:
		mutable SpinLock m_lock;

		bool m_is_64bit { false };

		uint64_t m_last_ticks	{ 0 };
		uint32_t m_32bit_wraps	{ 0 };

		uint32_t m_ticks_per_s	{ 0 };

		vaddr_t m_mmio_base { 0 };
	};

}
