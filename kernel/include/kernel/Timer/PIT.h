#pragma once

#include <kernel/Interruptable.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	class PIT final : public Timer, public Interruptable
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<PIT>> create();

		virtual uint64_t ms_since_boot() const override;
		virtual uint64_t ns_since_boot() const override;
		virtual timespec time_since_boot() const override;

		virtual void handle_irq() override;

	private:
		void initialize();
		uint64_t read_counter() const;

	private:
		mutable SpinLock m_lock;
		uint64_t m_system_time { 0 };
	};

}
