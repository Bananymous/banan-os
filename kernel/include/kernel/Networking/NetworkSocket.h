#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class NetworkSocket : public TmpInode, public BAN::Weakable<NetworkSocket>
	{
	public:
		static constexpr uint16_t PORT_NONE = 0;

	public:
		void bind_interface_and_port(NetworkInterface*, uint16_t port);
		~NetworkSocket();

		virtual BAN::ErrorOr<void> add_protocol_header(BAN::Vector<uint8_t>&, uint16_t src_port, uint16_t dst_port) = 0;
		virtual uint8_t protocol() const = 0;

	protected:
		NetworkSocket(mode_t mode, uid_t uid, gid_t gid);

		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<ssize_t> sendto_impl(const sys_sendto_t*) override;

	protected:
		NetworkInterface*	m_interface	= nullptr;
		uint16_t			m_port		= PORT_NONE;
	};

}
