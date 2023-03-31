#pragma once

#include <BAN/Memory.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{
	
	class FileSystem
	{
	public:
		virtual ~FileSystem() {}
		virtual BAN::RefPtr<Inode> root_inode() = 0;
	};

}