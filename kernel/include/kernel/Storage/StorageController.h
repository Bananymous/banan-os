#pragma once

namespace Kernel
{

	class StorageController
	{
	public:
		virtual ~StorageController() {}
		virtual BAN::ErrorOr<void> initialize() = 0;
	};

}