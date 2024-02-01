#pragma once

#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>

namespace Kernel
{

	class UDPSocket final : public NetworkSocket
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UDPSocket>> create(mode_t, uid_t, gid_t);

		void bind_interface(NetworkInterface*);

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

	private:
		UDPSocket(mode_t, uid_t, gid_t);

	private:
		NetworkInterface* m_interface = nullptr;

		friend class BAN::RefPtr<UDPSocket>;
	};

}
