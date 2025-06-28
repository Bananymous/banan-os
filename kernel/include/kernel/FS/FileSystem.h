#pragma once

#include <kernel/Device/Device.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{

	class FileSystem : public BAN::RefCounted<FileSystem>
	{
	public:
		virtual unsigned long bsize()   const = 0;
		virtual unsigned long frsize()  const = 0;
		virtual fsblkcnt_t    blocks()  const = 0;
		virtual fsblkcnt_t    bfree()   const = 0;
		virtual fsblkcnt_t    bavail()  const = 0;
		virtual fsfilcnt_t    files()   const = 0;
		virtual fsfilcnt_t    ffree()   const = 0;
		virtual fsfilcnt_t    favail()  const = 0;
		virtual unsigned long fsid()    const = 0;
		virtual unsigned long flag()    const = 0;
		virtual unsigned long namemax() const = 0;

		virtual ~FileSystem() {}

		static BAN::ErrorOr<BAN::RefPtr<FileSystem>> from_block_device(BAN::RefPtr<BlockDevice>);

		virtual BAN::RefPtr<Inode> root_inode() = 0;
	};

}
