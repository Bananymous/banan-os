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

	NetworkSocket::~NetworkSocket()
	{
	}

	void NetworkSocket::on_close_impl()
	{
		if (m_interface)
			NetworkManager::get().unbind_socket(m_port, this);
	}

	void NetworkSocket::bind_interface_and_port(NetworkInterface* interface, uint16_t port)
	{
		ASSERT(!m_interface);
		ASSERT(interface);
		m_interface = interface;
		m_port = port;
	}

	BAN::ErrorOr<void> NetworkSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		auto* addr_in = reinterpret_cast<const sockaddr_in*>(address);
		return NetworkManager::get().bind_socket(addr_in->sin_port, this);
	}

}
