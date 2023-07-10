#include <kernel/Device.h>
#include <kernel/FS/DevFS/FileSystem.h>

namespace Kernel
{

	Device::Device(mode_t mode, uid_t uid, gid_t gid)
		: RamInode(DevFileSystem::get(), mode, uid, gid)
	{ }

}