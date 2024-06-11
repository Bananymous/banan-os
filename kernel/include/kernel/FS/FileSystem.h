#pragma once

#include <kernel/Device/Device.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{

	class FileSystem : public BAN::RefCounted<FileSystem>
	{
	public:
		virtual ~FileSystem() {}

		static BAN::ErrorOr<BAN::RefPtr<FileSystem>> from_block_device(BAN::RefPtr<BlockDevice>);

		virtual BAN::RefPtr<Inode> root_inode() = 0;

		virtual dev_t dev() const = 0;
	};

}
