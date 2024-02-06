#pragma once

#include <BAN/Vector.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/Networking/IPv4Layer.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/PCI.h>

#include <netinet/in.h>

namespace Kernel
{

	class NetworkManager : public TmpFileSystem
	{
		BAN_NON_COPYABLE(NetworkManager);
		BAN_NON_MOVABLE(NetworkManager);

	public:
		static BAN::ErrorOr<void> initialize();
		static NetworkManager& get();

		BAN::ErrorOr<void> add_interface(PCI::Device& pci_device);

		BAN::Vector<BAN::RefPtr<NetworkInterface>> interfaces() { return m_interfaces; }

		BAN::ErrorOr<BAN::RefPtr<NetworkSocket>> create_socket(SocketDomain, SocketType, mode_t, uid_t, gid_t);

		void on_receive(NetworkInterface&, BAN::ConstByteSpan);

	private:
		NetworkManager();

	private:
		BAN::UniqPtr<IPv4Layer>						m_ipv4_layer;
		BAN::Vector<BAN::RefPtr<NetworkInterface>>	m_interfaces;
	};

}
