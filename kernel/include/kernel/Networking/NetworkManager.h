#pragma once

#include <BAN/Vector.h>
#include <kernel/FS/Socket.h>
#include <kernel/Networking/IPv4Layer.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/PCI.h>

#include <netinet/in.h>

namespace Kernel
{

	class NetworkManager
	{
		BAN_NON_COPYABLE(NetworkManager);
		BAN_NON_MOVABLE(NetworkManager);

	public:
		static BAN::ErrorOr<void> initialize();
		static NetworkManager& get();

		BAN::ErrorOr<void> add_interface(PCI::Device& pci_device);

		BAN::Vector<BAN::RefPtr<NetworkInterface>>& interfaces() { return m_interfaces; }

		BAN::ErrorOr<BAN::RefPtr<Socket>> create_socket(Socket::Domain, Socket::Type, mode_t, uid_t, gid_t);
		BAN::ErrorOr<void> connect_sockets(Socket::Domain, BAN::RefPtr<Socket>, BAN::RefPtr<Socket>);

		void on_receive(NetworkInterface&, BAN::ConstByteSpan);

	private:
		NetworkManager() {}

		BAN::ErrorOr<void> add_interface(BAN::RefPtr<NetworkInterface>);

	private:
		BAN::UniqPtr<IPv4Layer>						m_ipv4_layer;
		BAN::Vector<BAN::RefPtr<NetworkInterface>>	m_interfaces;

		friend class BAN::UniqPtr<NetworkManager>;
	};

}
