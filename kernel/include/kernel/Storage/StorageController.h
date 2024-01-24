#pragma once

#include <BAN/RefPtr.h>

namespace Kernel
{

	class StorageController : public BAN::RefCounted<StorageController>
	{
	public:
		virtual ~StorageController() {}
		virtual BAN::ErrorOr<void> initialize() = 0;
	};

}
