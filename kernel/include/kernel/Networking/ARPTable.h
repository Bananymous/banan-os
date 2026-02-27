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

		BAN::ErrorOr<BAN::MACAddress> get_mac_from_ipv4(NetworkInterface&, BAN::IPv4Address);

		BAN::ErrorOr<void> handle_arp_packet(NetworkInterface&, BAN::ConstByteSpan);

	private:
		ARPTable() = default;

	private:
		SpinLock m_arp_table_lock;
		BAN::HashMap<BAN::IPv4Address, BAN::MACAddress> m_arp_table;

		friend class BAN::UniqPtr<ARPTable>;
	};

}
