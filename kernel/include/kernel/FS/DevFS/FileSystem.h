#pragma once

#include <BAN/Vector.h>
#include <kernel/Device/Device.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	class DevFileSystem final : public TmpFileSystem
	{
	public:
		static void initialize();
		static DevFileSystem& get();
		
		void initialize_device_updater();

		void add_device(BAN::RefPtr<Device>);
		void add_inode(BAN::StringView path, BAN::RefPtr<TmpInode>);

		dev_t get_next_dev() const;
		int get_next_input_device() const;

		void initiate_sync(bool should_block);

	private:
		DevFileSystem()
			: TmpFileSystem(-1)
		{ }

	private:
		mutable SpinLock m_device_lock;

		BAN::Vector<BAN::RefPtr<Device>> m_devices;

		Semaphore m_sync_done;
		Semaphore m_sync_semaphore;
		volatile bool m_should_sync { false };
	};

}