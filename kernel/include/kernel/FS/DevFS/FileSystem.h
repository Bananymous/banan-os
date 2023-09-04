#pragma once

#include <kernel/Device/Device.h>
#include <kernel/FS/RamFS/FileSystem.h>

namespace Kernel
{
	
	class DevFileSystem final : public RamFileSystem
	{
	public:
		static void initialize();
		static DevFileSystem& get();
		
		void initialize_device_updater();
	
		void add_device(BAN::StringView path, BAN::RefPtr<RamInode>);

		dev_t get_next_dev();

	private:
		DevFileSystem(size_t size)
			: RamFileSystem(size)
		{ }

	private:
		SpinLock m_device_lock;
	};

}