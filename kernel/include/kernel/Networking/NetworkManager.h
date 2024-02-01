#pragma once

#include <BAN/Vector.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/PCI.h>

namespace Kernel
{

	class NetworkManager : public TmpFileSystem
	{
	public:
		enum class SocketType
		{
			STREAM,
			DGRAM,
			SEQPACKET,
		};

	public:
		static BAN::ErrorOr<void> initialize();
		static NetworkManager& get();

		BAN::ErrorOr<void> add_interface(PCI::Device& pci_device);
		BAN::ErrorOr<void> bind_socket(int port, BAN::RefPtr<NetworkSocket>);

		BAN::ErrorOr<BAN::RefPtr<NetworkSocket>> create_socket(SocketType, mode_t, uid_t, gid_t);

	private:
		NetworkManager();

	private:
		BAN::Vector<BAN::RefPtr<NetworkInterface>>		m_interfaces;
		BAN::HashMap<int, BAN::RefPtr<NetworkSocket>>	m_bound_sockets;
	};

}
