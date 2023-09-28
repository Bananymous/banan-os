#pragma once

#ifndef __is_kernel
#error "This is kernel only header"
#endif

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

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
		void reserve_address_space();

		BAN::ErrorOr<void> load_page_to_memory(Kernel::vaddr_t address);

		BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> clone(Kernel::PageTable&);

	private:
		LoadableELF(Kernel::PageTable&, BAN::RefPtr<Kernel::Inode>);
		BAN::ErrorOr<void> initialize();

	private:
		BAN::RefPtr<Kernel::Inode>			m_inode;
		Kernel::PageTable&					m_page_table;
		ElfNativeFileHeader					m_file_header;
		BAN::Vector<ElfNativeProgramHeader>	m_program_headers;
	};

}