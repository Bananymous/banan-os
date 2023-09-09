#include <kernel/Device/ZeroDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<ZeroDevice>> ZeroDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		auto* result = new ZeroDevice(mode, uid, gid, DevFileSystem::get().get_next_dev());
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<ZeroDevice>::adopt(result);
	}


	BAN::ErrorOr<size_t> ZeroDevice::read_impl(off_t, void* buffer, size_t bytes)
	{
		memset(buffer, 0, bytes);
		return bytes;
	}

}