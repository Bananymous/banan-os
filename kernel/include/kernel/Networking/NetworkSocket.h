#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/FS/Socket.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkLayer.h>

#include <netinet/in.h>

namespace Kernel
{

	enum NetworkProtocol : uint8_t
	{
		ICMP = 0x01,
		TCP = 0x06,
		UDP = 0x11,
	};

	class NetworkSocket : public Socket, public BAN::Weakable<NetworkSocket>
	{
		BAN_NON_COPYABLE(NetworkSocket);
		BAN_NON_MOVABLE(NetworkSocket);

	public:
		static constexpr uint16_t PORT_NONE = 0;

	public:
		void bind_address_and_port(const sockaddr*, socklen_t);
		~NetworkSocket();

		BAN::ErrorOr<BAN::RefPtr<NetworkInterface>> interface(const sockaddr* target, socklen_t target_len);

		virtual size_t protocol_header_size() const = 0;
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) = 0;
		virtual NetworkProtocol protocol() const = 0;

		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr* sender, socklen_t sender_len) = 0;

		bool is_bound() const { return m_address_len >= static_cast<socklen_t>(sizeof(sa_family_t)) && m_address.ss_family != AF_UNSPEC; }
		in_port_t bound_port() const
		{
			ASSERT(is_bound());
			ASSERT(m_address.ss_family == AF_INET && m_address_len >= static_cast<socklen_t>(sizeof(sockaddr_in)));
			return BAN::network_endian_to_host(reinterpret_cast<const sockaddr_in*>(&m_address)->sin_port);
		}

		const sockaddr* address() const { return reinterpret_cast<const sockaddr*>(&m_address); }
		socklen_t address_len() const { return m_address_len; }

	private:
		bool can_interface_send_to(const NetworkInterface&, const sockaddr*, socklen_t) const;

	protected:
		NetworkSocket(NetworkLayer&, const Socket::Info&);

		virtual BAN::ErrorOr<long> ioctl_impl(int request, void* arg) override;
		virtual BAN::ErrorOr<void> getsockname_impl(sockaddr*, socklen_t*) override;
		virtual BAN::ErrorOr<void> getpeername_impl(sockaddr*, socklen_t*) override = 0;

	protected:
		NetworkLayer&    m_network_layer;
		sockaddr_storage m_address       { .ss_family = AF_UNSPEC, .ss_storage = {} };
		socklen_t        m_address_len   { 0 };
	};

}
