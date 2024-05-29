#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/SharedMemoryObject.h>
#include <kernel/Random.h>

namespace Kernel
{

	static BAN::UniqPtr<SharedMemoryObjectManager> s_instance;

	BAN::ErrorOr<void> SharedMemoryObjectManager::initialize()
	{
		ASSERT(!s_instance);
		s_instance = TRY(BAN::UniqPtr<SharedMemoryObjectManager>::create());
		return {};
	}

	SharedMemoryObjectManager& SharedMemoryObjectManager::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	BAN::ErrorOr<SharedMemoryObjectManager::Key> SharedMemoryObjectManager::create_object(size_t size, PageTable::flags_t flags)
	{
		ASSERT(size % PAGE_SIZE == 0);

		auto object = TRY(BAN::RefPtr<Object>::create());
		object->size = size;
		object->flags = flags;
		TRY(object->paddrs.resize(size / PAGE_SIZE, 0));

		LockGuard _(m_mutex);

		Key key = Random::get<Key>();
		while (m_objects.contains(key))
			key = Random::get<Key>();

		TRY(m_objects.insert(key, object));
		return key;
	}

	BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> SharedMemoryObjectManager::map_object(Key key, PageTable& page_table, AddressRange address_range)
	{
		LockGuard _(m_mutex);

		auto it = m_objects.find(key);
		if (it == m_objects.end())
			return BAN::Error::from_errno(ENOENT);

		return TRY(SharedMemoryObject::create(it->value, page_table, address_range));
	}

	BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> SharedMemoryObject::create(BAN::RefPtr<SharedMemoryObjectManager::Object> object, PageTable& page_table, AddressRange address_range)
	{
		auto smo = TRY(BAN::UniqPtr<SharedMemoryObject>::create(object, page_table));
		TRY(smo->initialize(address_range));
		return BAN::move(smo);
	}

	BAN::ErrorOr<bool> SharedMemoryObject::allocate_page_containing_impl(vaddr_t address)
	{
		ASSERT(contains(address));

		// Check if address is already mapped
		vaddr_t vaddr = address & PAGE_ADDR_MASK;
		if (m_page_table.physical_address_of(vaddr) != 0)
			return false;

		SpinLockGuard _(m_object->spin_lock);

		paddr_t paddr = m_object->paddrs[(vaddr - m_vaddr) / PAGE_SIZE];
		if (paddr == 0)
		{
			paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			PageTable::with_fast_page(paddr, [&] {
				memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
			});
			m_object->paddrs[(vaddr - m_vaddr) / PAGE_SIZE] = paddr;
		}
		m_page_table.map_page_at(paddr, vaddr, m_flags);

		return true;
	}

}