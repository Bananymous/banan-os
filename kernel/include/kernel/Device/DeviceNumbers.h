#pragma once

#include <sys/types.h>

namespace Kernel
{

	enum class DeviceNumber : dev_t
	{
		Framebuffer = 1,
		TTY,
		Serial,
		Null,
		Zero,
		Debug,
		Input,
		SCSI,
		NVMeController,
		NVMeNamespace,
		Ethernet,
		TmpFS,
	};

}
