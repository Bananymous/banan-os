#pragma once

#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class StorageController : public CharacterDevice
	{
	public:
		virtual BAN::Vector<StorageDevice*> devices() = 0;
	};

}