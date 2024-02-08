#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/Socket.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkLayer.h>

#include <netinet/in.h>

namespace Kernel
{

	enum NetworkProtocol : uint8_t
	{
		ICMP = 0x01,
		UDP = 0x11,
	};

	class NetworkSocket : public TmpInode, public BAN::Weakable<NetworkSocket>
	{
		BAN_NON_COPYABLE(NetworkSocket);
		BAN_NON_MOVABLE(NetworkSocket);

	public:
		static constexpr uint16_t PORT_NONE = 0;

	public:
		void bind_interface_and_port(NetworkInterface*, uint16_t port);
		~NetworkSocket();

		NetworkInterface& interface() { ASSERT(m_interface); return *m_interface; }

		virtual size_t protocol_header_size() const = 0;
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port) = 0;
		virtual NetworkProtocol protocol() const = 0;

		virtual void add_packet(BAN::ConstByteSpan, BAN::IPv4Address sender_address, uint16_t sender_port) = 0;

	protected:
		NetworkSocket(NetworkLayer&, ino_t, const TmpInodeInfo&);

		virtual BAN::ErrorOr<size_t> read_packet(BAN::ByteSpan, sockaddr_in* sender_address) = 0;

		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<void> bind_impl(const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<size_t> sendto_impl(const sys_sendto_t*) override;
		virtual BAN::ErrorOr<size_t> recvfrom_impl(sys_recvfrom_t*) override;

		virtual BAN::ErrorOr<long> ioctl_impl(int request, void* arg) override;

	protected:
		NetworkLayer&		m_network_layer;
		NetworkInterface*	m_interface	= nullptr;
		uint16_t			m_port		= PORT_NONE;
	};

}
