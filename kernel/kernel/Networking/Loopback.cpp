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
			buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		loopback->set_ipv4_address({ 127, 0, 0, 1 });
		loopback->set_netmask({ 255, 0, 0, 0 });
		return loopback;
	}

	BAN::ErrorOr<void> LoopbackInterface::send_bytes(BAN::MACAddress destination, EtherType protocol, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() + sizeof(EthernetHeader) <= buffer_size);

		SpinLockGuard _(m_buffer_lock);

		uint8_t* buffer_vaddr = reinterpret_cast<uint8_t*>(m_buffer->vaddr());

		auto& ethernet_header = *reinterpret_cast<EthernetHeader*>(buffer_vaddr);
		ethernet_header.dst_mac = destination;
		ethernet_header.src_mac = get_mac_address();
		ethernet_header.ether_type = protocol;

		memcpy(buffer_vaddr + sizeof(EthernetHeader), buffer.data(), buffer.size());

		NetworkManager::get().on_receive(*this, BAN::ConstByteSpan {
			buffer_vaddr,
			buffer.size() + sizeof(EthernetHeader)
		});

		return {};
	}

}
