#include <BAN/Time.h>
#include <kernel/Device.h>
#include <kernel/RTC.h>

namespace Kernel
{

	Device::Device()
		: m_create_time({ BAN::to_unix_time(RTC::get_current_time()), 0 })
	{ }

}