#pragma once

#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class LoopbackInterface : public NetworkInterface
	{
	public:
		static constexpr size_t buffer_size = BAN::numeric_limits<uint16_t>::max() + 1;

	public:
		static BAN::ErrorOr<BAN::RefPtr<LoopbackInterface>> create();

		BAN::MACAddress get_mac_address() const override { return {}; }

		bool link_up() override { return true; }
		int link_speed() override { return 1000; }

		size_t payload_mtu() const override { return buffer_size - sizeof(EthernetHeader); }

	protected:
		LoopbackInterface()
			: NetworkInterface(Type::Loopback)
		{}

		BAN::ErrorOr<void> send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan) override;

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		SpinLock m_buffer_lock;
		BAN::UniqPtr<VirtualRange> m_buffer;
	};

}
