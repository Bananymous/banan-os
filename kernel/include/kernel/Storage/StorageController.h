#pragma once

#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class StorageController : public CharacterDevice
	{
	public:
		StorageController()
			: CharacterDevice(0660, 0, 0)
		{ }
		virtual BAN::Vector<BAN::RefPtr<StorageDevice>> devices() = 0;
	};

}