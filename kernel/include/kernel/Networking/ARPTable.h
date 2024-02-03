#pragma once

#include <BAN/HashMap.h>
#include <BAN/UniqPtr.h>
#include <kernel/Networking/NetworkInterface.h>

namespace Kernel
{

	class ARPTable
	{
		BAN_NON_COPYABLE(ARPTable);
		BAN_NON_MOVABLE(ARPTable);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<ARPTable>> create();

		BAN::ErrorOr<BAN::MACAddress> get_mac_from_ipv4(NetworkInterface&, BAN::IPv4Address);

		void handle_arp_packet(BAN::ConstByteSpan);

	private:
		ARPTable();

	private:
		struct ARPReply
		{
			BAN::IPv4Address ipv4_address { 0 };
			BAN::MACAddress mac_address;
		};

	private:
		SpinLock m_lock;

		BAN::HashMap<BAN::IPv4Address, BAN::MACAddress> m_arp_table;

		BAN::Atomic<bool>	m_has_got_reply;
		ARPReply			m_reply;

		friend class BAN::UniqPtr<ARPTable>;
	};

}
