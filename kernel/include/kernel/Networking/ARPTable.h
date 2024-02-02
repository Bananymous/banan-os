#pragma once

#include <BAN/HashMap.h>
#include <BAN/IPv4.h>
#include <BAN/MAC.h>
#include <BAN/UniqPtr.h>

namespace Kernel
{

	class ARPTable
	{
		BAN_NON_COPYABLE(ARPTable);
		BAN_NON_MOVABLE(ARPTable);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<ARPTable>> create();

		BAN::ErrorOr<BAN::MACAddress> get_mac_from_ipv4(BAN::IPv4Address);

	private:
		ARPTable();

	private:
		BAN::HashMap<BAN::IPv4Address, BAN::MACAddress> m_arp_table;

		friend class BAN::UniqPtr<ARPTable>;
	};

}
