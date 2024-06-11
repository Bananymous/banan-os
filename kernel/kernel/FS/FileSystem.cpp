#include <kernel/FS/Ext2/FileSystem.h>
#include <kernel/FS/FileSystem.h>

namespace Kernel
{

    BAN::ErrorOr<BAN::RefPtr<FileSystem>> FileSystem::from_block_device(BAN::RefPtr<BlockDevice> block_device)
    {
        if (auto res = Ext2FS::probe(block_device); !res.is_error() && res.value())
            return BAN::RefPtr<FileSystem>(TRY(Ext2FS::create(block_device)));
        dprintln("Unsupported filesystem");
        return BAN::Error::from_errno(ENOTSUP);
    }

}
