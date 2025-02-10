#pragma once

#include <BAN/Array.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel
{

	class USBController
	{
		// NOTE: Tier 0 == Root Hub

	public:
		USBController();

		uint8_t current_hub_init_tier() const;
		void register_hub_to_init(uint8_t tier);
		void mark_hub_init_done(uint8_t tier);

	private:
		mutable SpinLock m_hub_init_lock;
		uint8_t m_current_hub_init_tier { 0 };
		BAN::Array<uint32_t, 7> m_hubs_to_init_per_tier;
	};

}
