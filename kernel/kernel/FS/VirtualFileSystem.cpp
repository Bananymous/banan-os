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

}