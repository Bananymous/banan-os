#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/SharedMemoryObject.h>
#include <kernel/Process.h>
#include <kernel/Random.h>
#include <kernel/Timer/Timer.h>
#include <kernel/UserCopy.h>

#include <fcntl.h>

namespace Kernel
{

	static BAN::UniqPtr<SharedMemoryObjectManager> s_instance;

	class SharedMemoryObject : public MemoryRegion
	{
		BAN_NON_COPYABLE(SharedMemoryObject);
		BAN_NON_MOVABLE(SharedMemoryObject);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> create(BAN::RefPtr<SharedMemoryObjectManager::Object>, PageTable&, AddressRange, int status_flags);
		~SharedMemoryObject();

		BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) override;
		BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> split(size_t offset) override;

		BAN::ErrorOr<void> msync(vaddr_t, size_t, int) override { return {}; }

	protected:
		BAN::ErrorOr<bool> allocate_page_containing_impl(vaddr_t vaddr, bool wants_write) override;

	private:
		SharedMemoryObject(BAN::RefPtr<SharedMemoryObjectManager::Object>, PageTable& page_table, int status_flags);

	private:
		BAN::RefPtr<SharedMemoryObjectManager::Object> m_object;

		friend class BAN::UniqPtr<SharedMemoryObject>;
	};

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

	SharedMemoryObjectManager::Object::~Object()
	{
		for (auto paddr : paddrs)
			if (paddr)
				Heap::get().release_page(paddr);
	}

	BAN::ErrorOr<int> SharedMemoryObjectManager::shmget(key_t key, size_t size, int shmflg)
	{
		LockGuard _(m_mutex);

		if (key != IPC_PRIVATE)
		{
			if (auto it = m_ids.find(key); it != m_ids.end())
			{
				if (shmflg & IPC_EXCL)
					return BAN::Error::from_errno(EEXIST);

				const auto& object = m_objects[it->value];
				if (object->info.shm_segsz < size)
					return BAN::Error::from_errno(EINVAL);

				const int flags =
					(shmflg & S_IRUSR ? O_RDONLY : 0) |
					(shmflg & S_IWUSR ? O_WRONLY : 0);
				if (!object->can_current_process_access(flags))
					return BAN::Error::from_errno(EACCES);

				return it->value;
			}

			if (!(shmflg & IPC_CREAT))
				return BAN::Error::from_errno(ENOENT);
		}

		const auto& process = Process::current();
		const uid_t uid = process.credentials().euid();
		const gid_t gid = process.credentials().egid();
		const pid_t pid = process.pid();
		const mode_t mode = shmflg & 0777;

		const int shmid = ({
			int id;
			do {
				id = Random::get<unsigned>() & BAN::numeric_limits<int>::max();
			} while (m_ids.contains(shmid));
			id;
		});

		auto object = TRY(BAN::RefPtr<Object>::create(key, shmid, shmid_ds {
				.shm_perm = {
					.uid = uid,
					.gid = gid,
					.cuid = uid,
					.cgid = uid,
					.mode = mode,
				},
				.shm_segsz = size,
				.shm_lpid = 0,
				.shm_cpid = pid,
				.shm_nattch = 0,
				.shm_atime = 0,
				.shm_dtime = 0,
				.shm_ctime = SystemTimer::get().real_time().tv_sec,
		}));
		TRY(object->paddrs.resize(BAN::Math::div_round_up(size, PAGE_SIZE), 0));

		if (key != IPC_PRIVATE)
			TRY(m_ids.insert(key, shmid));

		if (auto ret = m_objects.insert(shmid, object); ret.is_error())
		{
			if (key != IPC_PRIVATE)
				m_ids.remove(key);
			return ret.release_error();
		}

		return shmid;
	}

	BAN::ErrorOr<void> SharedMemoryObjectManager::shmctl(int shmid, int cmd, struct shmid_ds* user_buf)
	{
		LockGuard _0(m_mutex);

		auto it = m_objects.find(shmid);
		if (it == m_objects.end())
			return BAN::Error::from_errno(EINVAL);

		LockGuard _1(it->value->mutex);

		switch (cmd)
		{
			case IPC_RMID:
				if (!it->value->can_current_process_access(O_WRONLY))
					return BAN::Error::from_errno(EACCES);
				if (it->key != IPC_PRIVATE)
					m_ids.remove(it->key);
				if (it->value->info.shm_nattch == 0)
					m_objects.remove(it);
				else
					it->value->marked_for_deletion = true;
				break;
			case IPC_STAT:
				TRY(write_to_user(user_buf, &it->value->info, sizeof(shmid_ds)));
				break;
			case IPC_SET:
			{
				if (!it->value->can_current_process_access(O_WRONLY))
					return BAN::Error::from_errno(EACCES);
				shmid_ds buf;
				TRY(read_from_user(user_buf, &buf, sizeof(shmid_ds)));
				it->value->info.shm_perm.uid  = buf.shm_perm.uid;
				it->value->info.shm_perm.gid  = buf.shm_perm.gid;
				it->value->info.shm_perm.mode = buf.shm_perm.mode & 0777;
				break;
			}
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		return {};
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> SharedMemoryObjectManager::shmat(int shmid, const void* shmaddr, int shmflg)
	{
		LockGuard _(m_mutex);

		auto it = m_objects.find(shmid);
		if (it == m_objects.end())
			return BAN::Error::from_errno(ENOENT);

		const int status_flags = (shmflg & SHM_RDONLY) ? O_RDONLY : O_RDWR;
		if (!it->value->can_current_process_access(status_flags))
			return BAN::Error::from_errno(EACCES);

		const AddressRange address_range = (shmaddr == nullptr)
			? AddressRange { .start = 0x400000, .end = USERSPACE_END }
			: TRY(Process::current().find_free_address_range(it->value->info.shm_segsz));

		auto region = TRY(SharedMemoryObject::create(it->value, Process::current().page_table(), address_range, status_flags));
		return BAN::UniqPtr<MemoryRegion>(BAN::move(region));
	}

	bool SharedMemoryObjectManager::Object::can_current_process_access(int flags) const
	{
		const auto& creds = Process::current().credentials();
		if (Inode::can_access(info.shm_perm.uid,  info.shm_perm.gid,  info.shm_perm.mode, creds, flags))
			return true;
		if (Inode::can_access(info.shm_perm.cuid, info.shm_perm.cgid, info.shm_perm.mode, creds, flags))
			return true;
		return false;
	}

	BAN::ErrorOr<BAN::UniqPtr<SharedMemoryObject>> SharedMemoryObject::create(BAN::RefPtr<SharedMemoryObjectManager::Object> object, PageTable& page_table, AddressRange address_range, int status_flags)
	{
		auto shared_memory_object = TRY(BAN::UniqPtr<SharedMemoryObject>::create(object, page_table, status_flags));
		TRY(shared_memory_object->initialize(address_range));
		return shared_memory_object;
	}

	SharedMemoryObject::SharedMemoryObject(BAN::RefPtr<SharedMemoryObjectManager::Object> object, PageTable& page_table, int status_flags)
		: MemoryRegion(
			page_table,
			object->info.shm_segsz,
			MemoryRegion::Type::SHARED,
			PageTable::UserSupervisor | ((status_flags & O_WRONLY) ? PageTable::ReadWrite : 0) | PageTable::Present,
			status_flags
		), m_object(object)
	{
		LockGuard _(m_object->mutex);
		m_object->info.shm_nattch++;
		m_object->info.shm_atime = SystemTimer::get().real_time().tv_sec;
		m_object->info.shm_lpid = Process::current().pid();
	}

	SharedMemoryObject::~SharedMemoryObject()
	{
		LockGuard _(m_object->mutex);
		if (--m_object->info.shm_nattch == 0 && m_object->marked_for_deletion)
			SharedMemoryObjectManager::get().m_objects.remove(m_object->id);
		m_object->info.shm_dtime = SystemTimer::get().real_time().tv_sec;
		m_object->info.shm_lpid = Process::current().pid();
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> SharedMemoryObject::clone(PageTable& new_page_table)
	{
		auto region = TRY(SharedMemoryObject::create(m_object, new_page_table, { .start = vaddr(), .end = vaddr() + size() }, m_status_flags));
		return BAN::UniqPtr<MemoryRegion>(BAN::move(region));
	}

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> SharedMemoryObject::split(size_t)
	{
		derrorln("SharedMemoryObjects are not splittable");
		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<bool> SharedMemoryObject::allocate_page_containing_impl(vaddr_t address, bool wants_write)
	{
		ASSERT(contains(address));
		(void)wants_write;

		const vaddr_t vaddr = address & PAGE_ADDR_MASK;
		if (m_page_table.physical_address_of(vaddr) != 0)
			return false;

		LockGuard _(m_object->mutex);

		paddr_t paddr = m_object->paddrs[(vaddr - m_vaddr) / PAGE_SIZE];
		if (paddr == 0)
		{
			paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			PageTable::with_per_cpu_fast_page(paddr, [&](void* addr) {
				memset(addr, 0x00, PAGE_SIZE);
			});
			m_object->paddrs[(vaddr - m_vaddr) / PAGE_SIZE] = paddr;
		}

		m_page_table.map_page_at(paddr, vaddr, m_flags);

		return true;
	}

}
