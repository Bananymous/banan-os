#include <kernel/Networking/ARPTable.h>

namespace Kernel
{

	static constexpr BAN::IPv4Address	s_broadcast_ipv4 { 0xFFFFFFFF };
	static constexpr BAN::MACAddress	s_broadcast_mac {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};

	BAN::ErrorOr<BAN::UniqPtr<ARPTable>> ARPTable::create()
	{
		return TRY(BAN::UniqPtr<ARPTable>::create());
	}

	ARPTable::ARPTable()
	{

	}

	BAN::ErrorOr<BAN::MACAddress> ARPTable::get_mac_from_ipv4(BAN::IPv4Address ipv4_address)
	{
		if (ipv4_address == s_broadcast_ipv4)
			return s_broadcast_mac;
		dprintln("No MAC for IPv4 {}", ipv4_address);
		return BAN::Error::from_errno(ENOTSUP);
	}

}
