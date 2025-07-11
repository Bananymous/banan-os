#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/HashMap.h>
#include <BAN/UniqPtr.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Thread.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	struct ARPPacket
	{
		BAN::NetworkEndian<uint16_t>	htype { 0 };
		BAN::NetworkEndian<uint16_t>	ptype { 0 };
		BAN::NetworkEndian<uint8_t>		hlen { 0 };
		BAN::NetworkEndian<uint8_t>		plen { 0 };
		BAN::NetworkEndian<uint16_t>	oper { 0 };
		BAN::MACAddress					sha { 0, 0, 0, 0, 0, 0 };
		BAN::IPv4Address				spa { 0 };
		BAN::MACAddress					tha { 0, 0, 0, 0, 0, 0 };
		BAN::IPv4Address				tpa { 0 };
	};
	static_assert(sizeof(ARPPacket) == 28);

	class ARPTable
	{
		BAN_NON_COPYABLE(ARPTable);
		BAN_NON_MOVABLE(ARPTable);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<ARPTable>> create();
		~ARPTable();

		BAN::ErrorOr<BAN::MACAddress> get_mac_from_ipv4(NetworkInterface&, BAN::IPv4Address);

		void add_arp_packet(NetworkInterface&, BAN::ConstByteSpan);

	private:
		ARPTable();

		void packet_handle_task();
		BAN::ErrorOr<void> handle_arp_packet(NetworkInterface&, const ARPPacket&);

	private:
		struct PendingArpPacket
		{
			NetworkInterface& interface;
			ARPPacket packet;
		};

	private:
		SpinLock m_table_lock;
		SpinLock m_pending_lock;

		BAN::HashMap<BAN::IPv4Address, BAN::MACAddress> m_arp_table;

		Thread*										m_thread { nullptr };
		BAN::CircularQueue<PendingArpPacket, 128>	m_pending_packets;
		ThreadBlocker								m_pending_thread_blocker;

		friend class BAN::UniqPtr<ARPTable>;
	};

}
