#include <kernel/USB/Controller.h>

namespace Kernel
{

	USBController::USBController()
	{
		for (auto& count : m_hubs_to_init_per_tier)
			count = 0;
		m_hubs_to_init_per_tier[0] = 1;
	}

	uint8_t USBController::current_hub_init_tier() const
	{
		SpinLockGuard _(m_hub_init_lock);
		return m_current_hub_init_tier;
	}

	void USBController::register_hub_to_init(uint8_t tier)
	{
		ASSERT(tier >= 1);
		ASSERT(tier <= m_hubs_to_init_per_tier.size());

		SpinLockGuard _(m_hub_init_lock);
		m_hubs_to_init_per_tier[tier]++;
		if (tier < m_current_hub_init_tier)
			m_current_hub_init_tier = tier;
	}

	void USBController::mark_hub_init_done(uint8_t tier)
	{
		ASSERT(tier < m_hubs_to_init_per_tier.size());

		SpinLockGuard _(m_hub_init_lock);
		m_hubs_to_init_per_tier[tier]--;
		if (tier == m_current_hub_init_tier && m_hubs_to_init_per_tier[tier] == 0)
			m_current_hub_init_tier++;
	}

}
