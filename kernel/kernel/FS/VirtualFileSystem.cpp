#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/FS/VirtualFileSystem.h>

namespace Kernel
{

	static VirtualFileSystem* s_instance = nullptr;

	void VirtualFileSystem::initialize(BAN::RefCounted<Inode> root_inode)
	{
		ASSERT(s_instance == nullptr);
		s_instance = new VirtualFileSystem(root_inode);
		ASSERT(s_instance);
	}
	
	VirtualFileSystem& VirtualFileSystem::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	bool VirtualFileSystem::is_initialized()
	{
		return s_instance != nullptr;
	}

	BAN::ErrorOr<BAN::RefCounted<Inode>> VirtualFileSystem::from_absolute_path(BAN::StringView path)
	{
		if (path.front() != '/')
			return BAN::Error::from_string("Path must be an absolute path");

		auto inode = root_inode();
		auto path_parts = TRY(path.split('/'));
		
		for (BAN::StringView part : path_parts)
			inode = TRY(inode->directory_find(part));
		
		return inode;
	}

}