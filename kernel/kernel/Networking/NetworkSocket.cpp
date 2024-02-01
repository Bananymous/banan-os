#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/NetworkSocket.h>

namespace Kernel
{

	NetworkSocket::NetworkSocket(mode_t mode, uid_t uid, gid_t gid)
		// FIXME: what the fuck is this
		: TmpInode(
			NetworkManager::get(),
			MUST(NetworkManager::get().allocate_inode(create_inode_info(mode, uid, gid))),
			create_inode_info(mode, uid, gid)
		)
	{ }

	void NetworkSocket::bind_interface(NetworkInterface* interface)
	{
		ASSERT(!m_interface);
		ASSERT(interface);
		m_interface = interface;
	}

}
