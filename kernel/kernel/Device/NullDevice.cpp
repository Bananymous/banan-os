#include <kernel/Device/NullDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<NullDevice>> NullDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		auto* result = new NullDevice(mode, uid, gid, DevFileSystem::get().get_next_dev());
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<NullDevice>::adopt(result);
	}

}