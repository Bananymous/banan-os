#pragma once

#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class LoopbackInterface : public NetworkInterface
	{
	public:
		static constexpr size_t buffer_size = BAN::numeric_limits<uint16_t>::max() + 1;
		static constexpr size_t buffer_count = 32;

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
		~LoopbackInterface();

		BAN::ErrorOr<void> send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::Span<const BAN::ConstByteSpan> payload) override;

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		void receive_thread();

	private:
		struct Descriptor
		{
			uint8_t* addr;
			uint32_t size;
			uint8_t state;
		};

	private:
		Mutex m_buffer_lock;
		BAN::UniqPtr<VirtualRange> m_buffer;

		uint32_t m_buffer_tail { 0 };
		uint32_t m_buffer_head { 0 };
		Descriptor m_descriptors[buffer_count] {};

		bool m_thread_should_die { false };
		BAN::Atomic<bool> m_thread_is_dead { true };
		ThreadBlocker m_thread_blocker;
	};

}
