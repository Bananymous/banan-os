#pragma once

#include <BAN/HashMap.h>
#include <BAN/UniqPtr.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	class SharedMemoryObject;

	class SharedMemoryObjectManager
	{
	public:
		using Key = uint32_t;

	public:
		static BAN::ErrorOr<void> initialize();
		static SharedMemoryObjectManager& get();

		BAN::ErrorOr<Key> create_object(size_t size, PageTable::flags_t);
		BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> map_object(Key, PageTable&, AddressRange);

	private:
		SharedMemoryObjectManager() {}

	private:
		struct Object : public BAN::RefCounted<Object>
		{
			size_t size;
			PageTable::flags_t flags;
			BAN::Vector<paddr_t> paddrs;
			SpinLock spin_lock;
		};

	private:
		Mutex m_mutex;
		BAN::HashMap<Key, BAN::RefPtr<Object>> m_objects;

		friend class SharedMemoryObject;
		friend class BAN::UniqPtr<SharedMemoryObjectManager>;
	};

	class SharedMemoryObject : public MemoryRegion
	{
		BAN_NON_COPYABLE(SharedMemoryObject);
		BAN_NON_MOVABLE(SharedMemoryObject);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> create(BAN::RefPtr<SharedMemoryObjectManager::Object>, PageTable&, AddressRange);

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) override { return BAN::Error::from_errno(ENOTSUP); }
		virtual BAN::ErrorOr<void> msync(vaddr_t, size_t, int) override { return {}; }

	protected:
		virtual BAN::ErrorOr<bool> allocate_page_containing_impl(vaddr_t vaddr) override;

	private:
		SharedMemoryObject(BAN::RefPtr<SharedMemoryObjectManager::Object> object, PageTable& page_table)
			: MemoryRegion(page_table, object->size, MemoryRegion::Type::SHARED, object->flags)
			, m_object(object)
		{ }

	private:
		BAN::RefPtr<SharedMemoryObjectManager::Object> m_object;

		friend class BAN::UniqPtr<SharedMemoryObject>;
	};

}
