#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Networking/ARPTable.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	enum ARPOperation : uint16_t
	{
		Request = 1,
		Reply = 2,
	};

	static constexpr BAN::IPv4Address	s_broadcast_ipv4 { 0xFFFFFFFF };
	static constexpr BAN::MACAddress	s_broadcast_mac {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};

	BAN::ErrorOr<BAN::UniqPtr<ARPTable>> ARPTable::create()
	{
		return TRY(BAN::UniqPtr<ARPTable>::create());
	}

	BAN::ErrorOr<BAN::MACAddress> ARPTable::get_mac_from_ipv4(NetworkInterface& interface, BAN::IPv4Address ipv4_address)
	{
		if (ipv4_address == s_broadcast_ipv4)
			return s_broadcast_mac;

		const auto netmask = interface.get_netmask();
		const bool same_subnet = ipv4_address.mask(netmask) == interface.get_ipv4_address().mask(netmask);

		if (interface.type() == NetworkInterface::Type::Loopback)
		{
			if (!same_subnet)
				return BAN::Error::from_errno(EADDRNOTAVAIL);
			return BAN::MACAddress {};
		}

		ASSERT(interface.type() == NetworkInterface::Type::Ethernet);

		if (interface.get_ipv4_address() == BAN::IPv4Address { 0 })
			return BAN::Error::from_errno(EINVAL);

		if (!same_subnet)
			ipv4_address = interface.get_gateway();

		{
			SpinLockGuard _(m_arp_table_lock);
			auto it = m_arp_table.find(ipv4_address);
			if (it != m_arp_table.end())
				return it->value;
		}

		ARPPacket arp_request;
		arp_request.htype = 0x0001;
		arp_request.ptype = EtherType::IPv4;
		arp_request.hlen = 0x06;
		arp_request.plen = 0x04;
		arp_request.oper = ARPOperation::Request;
		arp_request.sha = interface.get_mac_address();
		arp_request.spa = interface.get_ipv4_address();
		arp_request.tha = {{ 0, 0, 0, 0, 0, 0 }};
		arp_request.tpa = ipv4_address;

		TRY(interface.send_bytes(s_broadcast_mac, EtherType::ARP, BAN::ConstByteSpan::from(arp_request)));

		uint64_t timeout = SystemTimer::get().ms_since_boot() + 1'000;
		while (SystemTimer::get().ms_since_boot() < timeout)
		{
			{
				SpinLockGuard _(m_arp_table_lock);
				auto it = m_arp_table.find(ipv4_address);
				if (it != m_arp_table.end())
					return it->value;
			}
			Processor::yield();
		}

		return BAN::Error::from_errno(ETIMEDOUT);
	}

	BAN::ErrorOr<void> ARPTable::handle_arp_packet(NetworkInterface& interface, BAN::ConstByteSpan buffer)
	{
		if (buffer.size() < sizeof(ARPPacket))
		{
			dwarnln_if(DEBUG_ARP, "Too small ARP packet");
			return {};
		}

		const auto& packet = buffer.as<const ARPPacket>();

		if (packet.ptype != EtherType::IPv4)
		{
			dprintln("Non IPv4 arp packet?");
			return {};
		}

		switch (packet.oper)
		{
			case ARPOperation::Request:
			{
				if (packet.tpa == interface.get_ipv4_address())
				{
					const ARPPacket arp_reply {
						.htype = 0x0001,
						.ptype = EtherType::IPv4,
						.hlen = 0x06,
						.plen = 0x04,
						.oper = ARPOperation::Reply,
						.sha = interface.get_mac_address(),
						.spa = interface.get_ipv4_address(),
						.tha = packet.sha,
						.tpa = packet.spa,
					};
					TRY(interface.send_bytes(packet.sha, EtherType::ARP, BAN::ConstByteSpan::from(arp_reply)));
				}
				break;
			}
			case ARPOperation::Reply:
			{
				SpinLockGuard _(m_arp_table_lock);
				auto it = m_arp_table.find(packet.spa);

				if (it != m_arp_table.end())
				{
					if (it->value != packet.sha)
					{
						dprintln("Update IPv4 {} MAC to {}", packet.spa, packet.sha);
						it->value = packet.sha;
					}
				}
				else
				{
					TRY(m_arp_table.insert(packet.spa, packet.sha));
					dprintln("Assign IPv4 {} MAC to {}", packet.spa, packet.sha);
				}
				break;
			}
			default:
				dprintln("Unhandled ARP packet (oper {4H})", (uint16_t)packet.oper);
				break;
		}

		return {};
	}

}
