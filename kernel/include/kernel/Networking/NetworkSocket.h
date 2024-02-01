#pragma once

#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class NetworkSocket : public TmpInode
	{
	public:
		void bind_interface(NetworkInterface*);

	protected:
		NetworkSocket(mode_t mode, uid_t uid, gid_t gid);

	protected:
		NetworkInterface* m_interface = nullptr;
	};

}
