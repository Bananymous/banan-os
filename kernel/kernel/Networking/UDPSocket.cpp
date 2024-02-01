#include <kernel/Networking/UDPSocket.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<UDPSocket>> UDPSocket::create(mode_t mode, uid_t uid, gid_t gid)
	{
		return TRY(BAN::RefPtr<UDPSocket>::create(mode, uid, gid));
	}

	UDPSocket::UDPSocket(mode_t mode, uid_t uid, gid_t gid)
		: NetworkSocket(mode, uid, gid)
	{ }

	BAN::ErrorOr<size_t> UDPSocket::read_impl(off_t, BAN::ByteSpan)
	{
		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<size_t> UDPSocket::write_impl(off_t, BAN::ConstByteSpan)
	{
		return BAN::Error::from_errno(ENOTSUP);
	}

}
