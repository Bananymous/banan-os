#include <kernel/Device/DeviceNumbers.h>
#include <kernel/Device/ZeroDevice.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<ZeroDevice>> ZeroDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		static uint32_t minor = 0;
		auto* result = new ZeroDevice(mode, uid, gid, makedev(DeviceNumber::Zero, minor++));
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ZeroDevice>::adopt(result);
	}

	BAN::ErrorOr<size_t> ZeroDevice::read_impl(off_t, BAN::ByteSpan buffer)
	{
		memset(buffer.data(), 0, buffer.size());
		return buffer.size();
	}

}
