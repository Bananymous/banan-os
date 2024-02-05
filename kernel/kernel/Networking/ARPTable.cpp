#include <kernel/LockGuard.h>
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
		auto arp_table = TRY(BAN::UniqPtr<ARPTable>::create());
		arp_table->m_process = Process::create_kernel(
			[](void* arp_table_ptr)
			{
				auto& arp_table = *reinterpret_cast<ARPTable*>(arp_table_ptr);
				arp_table.packet_handle_task();
			}, arp_table.ptr()
		);
		ASSERT(arp_table->m_process);
		return arp_table;
	}

	ARPTable::ARPTable()
	{
	}

	ARPTable::~ARPTable()
	{
		if (m_process)
			m_process->exit(0, SIGKILL);
		m_process = nullptr;
	}

	BAN::ErrorOr<BAN::MACAddress> ARPTable::get_mac_from_ipv4(NetworkInterface& interface, BAN::IPv4Address ipv4_address)
	{
		if (ipv4_address == s_broadcast_ipv4)
			return s_broadcast_mac;

		{
			LockGuard _(m_lock);
			if (m_arp_table.contains(ipv4_address))
				return m_arp_table[ipv4_address];
		}

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

		uint64_t timeout = SystemTimer::get().ms_since_boot() + 1'000;
		while (SystemTimer::get().ms_since_boot() < timeout)
		{
			{
				LockGuard _(m_lock);
				if (m_arp_table.contains(ipv4_address))
					return m_arp_table[ipv4_address];
			}
			Scheduler::get().reschedule();
		}

		return BAN::Error::from_errno(ETIMEDOUT);
	}

	BAN::ErrorOr<void> ARPTable::handle_arp_packet(NetworkInterface& interface, const ARPPacket& packet)
	{
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
					BAN::Vector<uint8_t> full_packet_buffer;
					TRY(full_packet_buffer.resize(sizeof(ARPPacket) + sizeof(EthernetHeader)));
					auto full_packet = BAN::ByteSpan { full_packet_buffer.span() };

					auto& ethernet_header = full_packet.as<EthernetHeader>();
					ethernet_header.dst_mac = packet.sha;
					ethernet_header.src_mac = interface.get_mac_address();
					ethernet_header.ether_type = EtherType::ARP;

					auto& arp_request = full_packet.slice(sizeof(EthernetHeader)).as<ARPPacket>();
					arp_request.htype = 0x0001;
					arp_request.ptype = EtherType::IPv4;
					arp_request.hlen = 0x06;
					arp_request.plen = 0x04;
					arp_request.oper = ARPOperation::Reply;
					arp_request.sha = interface.get_mac_address();
					arp_request.spa = interface.get_ipv4_address();
					arp_request.tha = packet.sha;
					arp_request.tpa = packet.spa;

					TRY(interface.send_raw_bytes(full_packet));
				}
				break;
			}
			case ARPOperation::Reply:
			{
				LockGuard _(m_lock);
				if (m_arp_table.contains(packet.spa))
				{
					if (m_arp_table[packet.spa] != packet.sha)
					{
						dprintln("Update IPv4 {} MAC to {}", packet.spa, packet.sha);
						m_arp_table[packet.spa] = packet.sha;
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

	void ARPTable::packet_handle_task()
	{
		for (;;)
		{
			BAN::Optional<PendingArpPacket> pending;

			{
				CriticalScope _;
				if (!m_pending_packets.empty())
				{
					pending = m_pending_packets.front();
					m_pending_packets.pop();
				}
			}

			if (!pending.has_value())
			{
				m_pending_semaphore.block();
				continue;
			}

			if (auto ret = handle_arp_packet(pending->interface, pending->packet); ret.is_error())
				dwarnln("{}", ret.error());
		}
	}

	void ARPTable::add_arp_packet(NetworkInterface& interface, BAN::ConstByteSpan buffer)
	{
		auto& arp_packet = buffer.as<const ARPPacket>();

		if (m_pending_packets.full())
		{
			dprintln("arp packet queue full");
			return;
		}

		m_pending_packets.push({ .interface = interface, .packet = arp_packet });
		m_pending_semaphore.unblock();
	}

}
