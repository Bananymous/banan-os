#pragma once

#include <kernel/FS/FileSystem.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static void initialize(BAN::RefCounted<Inode> root_inode);
		static VirtualFileSystem& get();
		static bool is_initialized();

		virtual const BAN::RefCounted<Inode> root_inode() const override { return m_root_inode; }

	private:
		VirtualFileSystem(BAN::RefCounted<Inode> root_inode)
			: m_root_inode(root_inode)
		{}

	private:
		BAN::RefCounted<Inode> m_root_inode;
	};

}