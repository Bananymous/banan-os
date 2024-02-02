#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>

#include <netinet/in.h>

namespace Kernel
{

	class NetworkSocket : public TmpInode, public BAN::Weakable<NetworkSocket>
	{
	public:
		static constexpr uint16_t PORT_NONE = 0;

	public:
		void bind_interface_and_port(NetworkInterface*, uint16_t port);
		~NetworkSocket();

		virtual size_t protocol_header_size() const = 0;
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t src_port, uint16_t dst_port) = 0;
		virtual uint8_t protocol() const = 0;

		virtual void add_packet(BAN::ConstByteSpan, BAN::IPv4Address sender_address, uint16_t sender_port) = 0;

	protected:
		NetworkSocket(mode_t mode, uid_t uid, gid_t gid);

		virtual BAN::ErrorOr<size_t> read_packet(BAN::ByteSpan, sockaddr_in* sender_address) = 0;

		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<ssize_t> sendto_impl(const sys_sendto_t*) override;
		virtual BAN::ErrorOr<ssize_t> recvfrom_impl(sys_recvfrom_t*) override;

		virtual BAN::ErrorOr<long> ioctl_impl(int request, void* arg) override;

	protected:
		NetworkInterface*	m_interface	= nullptr;
		uint16_t			m_port		= PORT_NONE;
	};

}
