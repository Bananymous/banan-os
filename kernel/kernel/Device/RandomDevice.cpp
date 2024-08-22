#include <kernel/Device/DeviceNumbers.h>
#include <kernel/Device/RandomDevice.h>
#include <kernel/Random.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<RandomDevice>> RandomDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		static uint32_t minor = 0;
		auto* result = new RandomDevice(mode, uid, gid, makedev(DeviceNumber::Random, minor++));
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<RandomDevice>::adopt(result);
	}

	BAN::ErrorOr<size_t> RandomDevice::read_impl(off_t, BAN::ByteSpan buffer)
	{
		size_t nwritten = 0;
		while (nwritten < buffer.size())
		{
			const uint64_t random = Random::get_u64();
			const size_t to_copy = BAN::Math::min(buffer.size() - nwritten, sizeof(random));
			memcpy(buffer.data() + nwritten, &random, to_copy);
			nwritten += to_copy;
		}
		return buffer.size();
	}

}
