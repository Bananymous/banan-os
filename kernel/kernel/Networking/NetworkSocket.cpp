#include <kernel/Networking/IPv4.h>
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

	BAN::ErrorOr<ssize_t> NetworkSocket::sendto_impl(const sys_sendto_t* arguments)
	{
		if (arguments->dest_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		if (arguments->flags)
		{
			dprintln("flags not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (!m_interface)
			TRY(NetworkManager::get().bind_socket(PORT_NONE, this));

		auto* destination = reinterpret_cast<const sockaddr_in*>(arguments->dest_addr);
		auto  message = BAN::ConstByteSpan((const uint8_t*)arguments->message, arguments->length);

		if (destination->sin_port == PORT_NONE)
			return BAN::Error::from_errno(EINVAL);

		if (destination->sin_addr.s_addr != 0xFFFFFFFF)
		{
			dprintln("Only broadcast ip supported");
			return BAN::Error::from_errno(EINVAL);
		}

		static uint8_t dest_mac[6] { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

		BAN::Vector<uint8_t> full_packet;
		TRY(full_packet.resize(message.size()));
		memcpy(full_packet.data(), message.data(), message.size());
		TRY(add_protocol_header(full_packet, m_port, destination->sin_port));
		TRY(add_ipv4_header(full_packet, m_interface->get_ipv4_address(), destination->sin_addr.s_addr, protocol()));
		TRY(m_interface->add_interface_header(full_packet, dest_mac));

		TRY(m_interface->send_raw_bytes(BAN::ConstByteSpan { full_packet.span() }));

		return arguments->length;
	}

}
