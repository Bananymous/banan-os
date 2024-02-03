#include <kernel/LockGuard.h>
#include <kernel/Networking/ARPTable.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	struct ARPPacket
	{
		BAN::NetworkEndian<uint16_t>	htype;
		BAN::NetworkEndian<uint16_t>	ptype;
		BAN::NetworkEndian<uint8_t>		hlen;
		BAN::NetworkEndian<uint8_t>		plen;
		BAN::NetworkEndian<uint16_t>	oper;
		BAN::MACAddress					sha;
		BAN::IPv4Address				spa;
		BAN::MACAddress					tha;
		BAN::IPv4Address				tpa;
	};
	static_assert(sizeof(ARPPacket) == 28);

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

	ARPTable::ARPTable()
	{

	}

	BAN::ErrorOr<BAN::MACAddress> ARPTable::get_mac_from_ipv4(NetworkInterface& interface, BAN::IPv4Address ipv4_address)
	{
		LockGuard _(m_lock);

		if (ipv4_address == s_broadcast_ipv4)
			return s_broadcast_mac;
		if (m_arp_table.contains(ipv4_address))
			return m_arp_table[ipv4_address];

		BAN::Vector<uint8_t> full_packet_buffer;
		TRY(full_packet_buffer.resize(sizeof(ARPPacket) + sizeof(EthernetHeader)));
		auto full_packet = BAN::ByteSpan { full_packet_buffer.span() };

		auto& ethernet_header = full_packet.as<EthernetHeader>();
		ethernet_header.dst_mac = s_broadcast_mac;
		ethernet_header.src_mac = interface.get_mac_address();
		ethernet_header.ether_type = EtherType::ARP;

		auto& arp_request = full_packet.slice(sizeof(EthernetHeader)).as<ARPPacket>();
		arp_request.htype = 0x0001;
		arp_request.ptype = EtherType::IPv4;
		arp_request.hlen = 0x06;
		arp_request.plen = 0x04;
		arp_request.oper = ARPOperation::Request;
		arp_request.sha = interface.get_mac_address();
		arp_request.spa = interface.get_ipv4_address();
		arp_request.tha = {{ 0, 0, 0, 0, 0, 0 }};
		arp_request.tpa = ipv4_address;

		TRY(interface.send_raw_bytes(full_packet));

		uint64_t timeout = SystemTimer::get().ms_since_boot() + 5'000;
		while (!m_has_got_reply)
			if (SystemTimer::get().ms_since_boot() >= timeout)
				return BAN::Error::from_errno(ETIMEDOUT);
		ASSERT_EQ(m_reply.ipv4_address, ipv4_address);

		BAN::MACAddress mac_address = m_reply.mac_address;
		(void)m_arp_table.insert(ipv4_address, m_reply.mac_address);
		m_has_got_reply = false;

		dprintln("IPv4 {} resolved to MAC {}", ipv4_address, mac_address);

		return mac_address;
	}

	void ARPTable::handle_arp_packet(BAN::ConstByteSpan buffer)
	{
		auto& arp_packet = buffer.as<const ARPPacket>();
		if (arp_packet.oper != ARPOperation::Reply)
		{
			dprintln("Unhandled non-rely ARP packet (operation {2H})", (uint16_t)arp_packet.oper);
			return;
		}

		if (arp_packet.ptype != EtherType::IPv4)
		{
			dprintln("Unhandled non-ipv4 ARP packet (ptype {2H})", (uint16_t)arp_packet.ptype);
			return;
		}

		ASSERT(!m_has_got_reply);
		m_has_got_reply = true;
		m_reply.ipv4_address = arp_packet.spa;
		m_reply.mac_address = arp_packet.sha;
	}

}
