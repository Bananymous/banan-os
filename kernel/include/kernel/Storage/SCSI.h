#pragma once

#include <sys/types.h>

namespace Kernel
{

	dev_t scsi_get_rdev();
	void scsi_free_rdev(dev_t);

}
