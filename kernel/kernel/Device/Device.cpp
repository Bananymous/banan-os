#include <kernel/Device/Device.h>
#include <kernel/FS/DevFS/FileSystem.h>

namespace Kernel
{

	Device::Device(mode_t mode, uid_t uid, gid_t gid)
		// FIXME: what the fuck is this
		: TmpInode(
			DevFileSystem::get(),
			MUST(DevFileSystem::get().allocate_inode(create_inode_info(mode, uid, gid))),
			create_inode_info(mode, uid, gid)
		)
	{ }

}