#pragma once

#include <kernel/BootInfo.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{

	BAN::ErrorOr<bool> unpack_boot_module_into_directory(BAN::RefPtr<Inode>, const BootModule&);

}
