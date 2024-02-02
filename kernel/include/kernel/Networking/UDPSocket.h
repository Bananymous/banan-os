#pragma once

#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>

namespace Kernel
{

	class UDPSocket final : public NetworkSocket
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UDPSocket>> create(mode_t, uid_t, gid_t);

		virtual BAN::ErrorOr<void> add_protocol_header(BAN::Vector<uint8_t>&, uint16_t src_port, uint16_t dst_port) override;
		virtual uint8_t protocol() const override { return 0x11; }

	private:
		UDPSocket(mode_t, uid_t, gid_t);

	private:
		friend class BAN::RefPtr<UDPSocket>;
	};

}
