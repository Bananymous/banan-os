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
		static BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> load_from_inode(Kernel::PageTable&, const Kernel::Credentials&, BAN::RefPtr<Kernel::Inode>);
		~LoadableELF();

		Kernel::vaddr_t entry_point() const;

		bool has_interpreter() const { return !!m_interpreter.inode; }
		BAN::RefPtr<Kernel::Inode> inode() { return m_executable.inode; }

		bool contains(Kernel::vaddr_t address) const;
		bool is_address_space_free() const;
		void reserve_address_space();

		void update_suid_sgid(Kernel::Credentials&);

		BAN::ErrorOr<void> load_page_to_memory(Kernel::vaddr_t address);

		BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> clone(Kernel::PageTable&);

		size_t virtual_page_count() const { return m_virtual_page_count; }
		size_t physical_page_count() const { return m_physical_page_count; }

	private:
		struct LoadableElfFile
		{
			BAN::RefPtr<Kernel::Inode> inode;
			ElfNativeFileHeader file_header;
			BAN::Vector<ElfNativeProgramHeader>	program_headers;
			Kernel::vaddr_t dynamic_base;
		};

		struct LoadResult
		{
			LoadableElfFile elf_file;
			BAN::RefPtr<Kernel::Inode> interp;
		};

	private:
		LoadableELF(Kernel::PageTable&);
		BAN::ErrorOr<void> initialize(const Kernel::Credentials&, BAN::RefPtr<Kernel::Inode>);

		bool does_executable_and_interpreter_overlap() const;
		BAN::ErrorOr<LoadResult> load_elf_file(const Kernel::Credentials&, BAN::RefPtr<Kernel::Inode>) const;

	private:
		LoadableElfFile    m_executable;
		LoadableElfFile    m_interpreter;
		Kernel::PageTable& m_page_table;
		size_t             m_virtual_page_count  { 0 };
		size_t             m_physical_page_count { 0 };
		bool               m_is_loaded           { false };

		friend class BAN::UniqPtr<LoadableELF>;
	};

}
