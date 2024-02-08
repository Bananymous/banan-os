#pragma once

namespace Kernel
{

	enum class SocketDomain
	{
		INET,
		INET6,
		UNIX,
	};

	enum class SocketType
	{
		STREAM,
		DGRAM,
		SEQPACKET,
	};

}
