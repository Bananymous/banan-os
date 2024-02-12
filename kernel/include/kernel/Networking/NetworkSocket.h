#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/Socket.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkLayer.h>

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
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) = 0;
		virtual NetworkProtocol protocol() const = 0;

		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr_storage& sender) = 0;

		bool is_bound() const { return m_interface != nullptr; }

	protected:
		NetworkSocket(NetworkLayer&, ino_t, const TmpInodeInfo&);

		virtual BAN::ErrorOr<long> ioctl_impl(int request, void* arg) override;

	protected:
		NetworkLayer&		m_network_layer;
		NetworkInterface*	m_interface	= nullptr;
		uint16_t			m_port { PORT_NONE };
	};

}
