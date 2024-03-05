#pragma once

#include <kernel/Interruptable.h>
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

	private:
		volatile uint64_t m_system_time { 0 };
	};

}
