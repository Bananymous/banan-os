#pragma once

#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class NetworkSocket;
	enum class SocketType;

	class NetworkLayer
	{
	public:
		virtual ~NetworkLayer() {}

		virtual void unbind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) = 0;
		virtual BAN::ErrorOr<void> bind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) = 0;

		virtual BAN::ErrorOr<size_t> sendto(NetworkSocket&, const sys_sendto_t*) = 0;

	protected:
		NetworkLayer() = default;
	};

}
