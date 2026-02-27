#include <kernel/Lock/LockGuard.h>
#include <kernel/Networking/Loopback.h>
#include <kernel/Networking/NetworkManager.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<LoopbackInterface>> LoopbackInterface::create()
	{
		auto* loopback_ptr = new LoopbackInterface();
		if (loopback_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto loopback = BAN::RefPtr<LoopbackInterface>::adopt(loopback_ptr);

		loopback->m_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			BAN::numeric_limits<vaddr_t>::max(),
			buffer_size * buffer_count,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, false
		));

		auto* thread = TRY(Thread::create_kernel([](void* loopback_ptr) {
			static_cast<LoopbackInterface*>(loopback_ptr)->receive_thread();
		}, loopback_ptr));
		if (auto ret = Processor::scheduler().add_thread(thread); ret.is_error())
		{
			delete thread;
			return ret.release_error();
		}
		loopback->m_thread_is_dead = false;

		loopback->set_ipv4_address({ 127, 0, 0, 1 });
		loopback->set_netmask({ 255, 0, 0, 0 });

		for (size_t i = 0; i < buffer_count; i++)
		{
			loopback->m_descriptors[i] = {
				.addr = reinterpret_cast<uint8_t*>(loopback->m_buffer->vaddr()) + i * buffer_size,
				.size = 0,
				.state = 0,
			};
		}

		return loopback;
	}

	LoopbackInterface::~LoopbackInterface()
	{
		m_thread_should_die = true;
		m_thread_blocker.unblock();

		while (!m_thread_is_dead)
			Processor::yield();
	}

	BAN::ErrorOr<void> LoopbackInterface::send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::Span<const BAN::ConstByteSpan> payload)
	{
		auto& descriptor =
			[&]() -> Descriptor&
			{
				LockGuard _(m_buffer_lock);
				for (;;)
				{
					auto& descriptor = m_descriptors[m_buffer_head];
					if (descriptor.state == 0)
					{
						m_buffer_head = (m_buffer_head + 1) % buffer_count;
						descriptor.state = 1;
						return descriptor;
					}
					m_thread_blocker.block_indefinite(&m_buffer_lock);
				}
			}();

		auto& ethernet_header = *reinterpret_cast<EthernetHeader*>(descriptor.addr);
		ethernet_header.dst_mac = destination;
		ethernet_header.src_mac = get_mac_address();
		ethernet_header.ether_type = protocol;

		size_t packet_size = sizeof(EthernetHeader);
		for (const auto& buffer : payload)
		{
			ASSERT(packet_size + buffer.size() <= buffer_size);
			memcpy(descriptor.addr + packet_size, buffer.data(), buffer.size());
			packet_size += buffer.size();
		}

		LockGuard _(m_buffer_lock);
		descriptor.size = packet_size;
		descriptor.state = 2;
		m_thread_blocker.unblock();

		return {};
	}

	void LoopbackInterface::receive_thread()
	{
		LockGuard _(m_buffer_lock);

		while (!m_thread_should_die)
		{
			for (;;)
			{
				auto& descriptor = m_descriptors[m_buffer_tail];
				if (descriptor.state != 2)
					break;
				m_buffer_tail = (m_buffer_tail + 1) % buffer_count;

				m_buffer_lock.unlock();

				NetworkManager::get().on_receive(*this, {
					descriptor.addr,
					descriptor.size,
				});

				m_buffer_lock.lock();

				descriptor.size = 0;
				descriptor.state = 0;
				m_thread_blocker.unblock();
			}

			m_thread_blocker.block_indefinite(&m_buffer_lock);
		}

		m_thread_is_dead = true;
	}

}
