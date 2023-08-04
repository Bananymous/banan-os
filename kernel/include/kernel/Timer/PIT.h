#pragma once

#include <kernel/Timer/Timer.h>
#include <stdint.h>

#define PIT_IRQ 0

namespace Kernel
{

	class PIT final : public Timer
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<PIT>> create();

		virtual uint64_t ms_since_boot() const override;
		virtual timespec time_since_boot() const override;

	private:
		void initialize();
	};

}