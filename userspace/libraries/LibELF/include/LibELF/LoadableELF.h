#pragma once

#ifndef __is_kernel
#error "This is kernel only header"
#endif

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

#include <kernel/Credentials.h>
#include <kernel/FS/Inode.h>
#include <kernel/Memory/PageTable.h>

#include <LibELF/Types.h>

namespace LibELF
{

	class LoadableELF
	{
		BAN_NON_COPYABLE(LoadableELF);
		BAN_NON_MOVABLE(LoadableELF);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> load_from_inode(Kernel::PageTable&, BAN::RefPtr<Kernel::Inode>);
		~LoadableELF();

		Kernel::vaddr_t entry_point() const;

		bool contains(Kernel::vaddr_t address) const;
		bool is_address_space_free() const;
		void reserve_address_space();

		void update_suid_sgid(Kernel::Credentials&);

		BAN::ErrorOr<void> load_page_to_memory(Kernel::vaddr_t address);

		BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> clone(Kernel::PageTable&);

		size_t virtual_page_count() const { return m_virtual_page_count; }
		size_t physical_page_count() const { return m_physical_page_count; }

	private:
		LoadableELF(Kernel::PageTable&, BAN::RefPtr<Kernel::Inode>);
		BAN::ErrorOr<void> initialize();

	private:
		BAN::RefPtr<Kernel::Inode>			m_inode;
		Kernel::PageTable&					m_page_table;
		ElfNativeFileHeader					m_file_header;
		BAN::Vector<ElfNativeProgramHeader>	m_program_headers;
		size_t m_virtual_page_count = 0;
		size_t m_physical_page_count = 0;
		bool m_loaded { false };
	};

}
