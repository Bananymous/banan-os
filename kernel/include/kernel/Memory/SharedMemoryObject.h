#pragma once

#include <BAN/HashMap.h>
#include <BAN/UniqPtr.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/MemoryRegion.h>

#include <sys/shm.h>

namespace Kernel
{

	class SharedMemoryObjectManager
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static SharedMemoryObjectManager& get();

		BAN::ErrorOr<int> shmget(key_t key, size_t size, int shmflg);
		BAN::ErrorOr<void> shmctl(int shmid, int cmd, struct shmid_ds* user_buf);
		BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> shmat(int shmid, const void* shmaddr, int shmflg);

	private:
		SharedMemoryObjectManager() {}

	private:
		struct Object : public BAN::RefCounted<Object>
		{
			Object(key_t key, shmid_ds info)
				: key(key)
				, info(info)
			{ }
			~Object();

			bool can_current_process_access(int flags) const;

			const key_t key;
			shmid_ds info;

			Mutex mutex;
			BAN::Vector<paddr_t> paddrs;
			bool marked_for_deletion { false };
		};

	private:
		Mutex m_mutex;
		BAN::HashMap<key_t, int> m_ids;
		BAN::HashMap<int, BAN::RefPtr<Object>> m_objects;

		friend class SharedMemoryObject;
		friend class BAN::UniqPtr<SharedMemoryObjectManager>;
	};

}
