#include <BAN/Endianness.h>
#include <kernel/Networking/UDPSocket.h>

namespace Kernel
{

	struct UDPHeader
	{
		BAN::NetworkEndian<uint16_t> src_port;
		BAN::NetworkEndian<uint16_t> dst_port;
		BAN::NetworkEndian<uint16_t> length;
		BAN::NetworkEndian<uint16_t> checksum;
	};
	static_assert(sizeof(UDPHeader) == 8);

	BAN::ErrorOr<BAN::RefPtr<UDPSocket>> UDPSocket::create(mode_t mode, uid_t uid, gid_t gid)
	{
		return TRY(BAN::RefPtr<UDPSocket>::create(mode, uid, gid));
	}

	UDPSocket::UDPSocket(mode_t mode, uid_t uid, gid_t gid)
		: NetworkSocket(mode, uid, gid)
	{ }

	BAN::ErrorOr<void> UDPSocket::add_protocol_header(BAN::Vector<uint8_t>& packet, uint16_t src_port, uint16_t dst_port)
	{
		TRY(packet.resize(packet.size() + sizeof(UDPHeader)));
		memmove(packet.data() + sizeof(UDPHeader), packet.data(), packet.size() - sizeof(UDPHeader));

		auto* header = reinterpret_cast<UDPHeader*>(packet.data());
		header->src_port = src_port;
		header->dst_port = dst_port;
		header->length = packet.size();
		header->checksum = 0;

		return {};
	}

}
