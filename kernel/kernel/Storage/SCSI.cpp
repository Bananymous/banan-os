#include <kernel/Device/DeviceNumbers.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Storage/SCSI.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static uint64_t s_scsi_bitmap { 0 };
	static SpinLock s_scsi_spinlock;

	static constexpr size_t s_scsi_bitmap_bits = sizeof(s_scsi_bitmap) * 8;

	dev_t scsi_get_rdev()
	{
		SpinLockGuard _(s_scsi_spinlock);

		uint64_t mask = 1;
		for (uint8_t minor = 0; minor < s_scsi_bitmap_bits; minor++, mask <<= 1)
		{
			if (s_scsi_bitmap & mask)
				continue;
			s_scsi_bitmap |= mask;

			return makedev(DeviceNumber::SCSI, minor);
		}

		ASSERT_NOT_REACHED();
	}

	void scsi_free_rdev(dev_t rdev)
	{
		ASSERT(major(rdev) == static_cast<dev_t>(DeviceNumber::SCSI));

		SpinLockGuard _(s_scsi_spinlock);

		const uint64_t mask = static_cast<uint64_t>(1) << minor(rdev);
		ASSERT(s_scsi_bitmap & mask);

		s_scsi_bitmap &= ~mask;
	}

}
