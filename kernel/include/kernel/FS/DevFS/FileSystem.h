#pragma once

#include <kernel/Device/Device.h>
#include <kernel/FS/RamFS/FileSystem.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	class DevFileSystem final : public RamFileSystem
	{
	public:
		static void initialize();
		static DevFileSystem& get();
		
		void initialize_device_updater();

		void add_device(BAN::StringView path, BAN::RefPtr<RamInode>);
		void for_each_device(const BAN::Function<BAN::Iteration(Device*)>& callback);

		dev_t get_next_dev();

		void initiate_sync(bool should_block);

	private:
		DevFileSystem(size_t size)
			: RamFileSystem(size)
		{ }

	private:
		SpinLock m_device_lock;

		Semaphore m_sync_done;
		Semaphore m_sync_semaphore;
		volatile bool m_should_sync { false };
	};

}