#include <kernel/Memory/VirtualRange.h>

#include <sys/mman.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<VirtualRange>> VirtualRange::create_to_vaddr_range(PageTable&, AddressRange, size_t size, PageTable::flags_t flags, bool)
	{
		const int prot =
			((flags & PageTable::Flags::Present)   ? PROT_READ  : 0) |
			((flags & PageTable::Flags::ReadWrite) ? PROT_WRITE : 0) |
			((flags & PageTable::Flags::Execute)   ? PROT_EXEC  : 0);

		void* addr = mmap(nullptr, size, prot, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (addr == MAP_FAILED)
			return BAN::Error::from_errno(ENOMEM);

		return BAN::UniqPtr<VirtualRange>::adopt(new VirtualRange(reinterpret_cast<vaddr_t>(addr), size));
	}

	VirtualRange::VirtualRange(vaddr_t vaddr, size_t size)
		: m_vaddr(vaddr)
		, m_size(size)
	{ }

	VirtualRange::~VirtualRange()
	{
		munmap(reinterpret_cast<void*>(m_vaddr), m_size);
	}

}
