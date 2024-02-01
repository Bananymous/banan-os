#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class NetworkSocket : public TmpInode, public BAN::Weakable<NetworkSocket>
	{
	public:
		void bind_interface_and_port(NetworkInterface*, uint16_t port);
		~NetworkSocket();

	protected:
		NetworkSocket(mode_t mode, uid_t uid, gid_t gid);

		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;

	protected:
		NetworkInterface*	m_interface = nullptr;
		uint16_t			m_port = 0;
	};

}
