#pragma once

#include <BAN/Memory.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{
	
	class FileSystem
	{
	public:
		virtual BAN::RefPtr<Inode> root_inode() = 0;
	};

}