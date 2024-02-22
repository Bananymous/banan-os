#include <kernel/Device/DeviceNumbers.h>
#include <kernel/Device/NullDevice.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<NullDevice>> NullDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		static uint32_t minor = 0;
		auto* result = new NullDevice(mode, uid, gid, makedev(DeviceNumber::Null, minor++));
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<NullDevice>::adopt(result);
	}

}
