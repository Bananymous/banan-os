#pragma once

#ifdef __is_kernel
#include <kernel/FS/Inode.h>
#include <kernel/Memory/VirtualRange.h>
#endif

#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Arch.h>
#include "Types.h"

namespace LibELF
{

	class ELF
	{
	public:
#ifdef __is_kernel
		static BAN::ErrorOr<BAN::UniqPtr<ELF>> load_from_file(BAN::RefPtr<Kernel::Inode>);
#else
		static BAN::ErrorOr<BAN::UniqPtr<ELF>> load_from_file(BAN::StringView);
#endif

		const Elf64FileHeader& file_header64() const;
		const Elf64ProgramHeader& program_header64(size_t) const;
		const Elf64SectionHeader& section_header64(size_t) const;
		const char* lookup_section_name64(uint32_t) const;
		const char* lookup_string64(size_t, uint32_t) const;
#if ARCH(x86_64)
		const Elf64FileHeader& file_header_native() const { return file_header64(); }
		const Elf64ProgramHeader& program_header_native(size_t index) const { return program_header64(index); }
		const Elf64SectionHeader& section_header_native(size_t index) const { return section_header64(index); }
		const char* lookup_section_name_native(uint32_t offset) const { return lookup_section_name64(offset); }
		const char* lookup_string_native(size_t table_index, uint32_t offset) const { return lookup_string64(table_index, offset); }
		bool is_native() const { return is_x86_64(); }
#endif

		const Elf32FileHeader& file_header32() const;
		const Elf32ProgramHeader& program_header32(size_t) const;
		const Elf32SectionHeader& section_header32(size_t) const;
		const char* lookup_section_name32(uint32_t) const;
		const char* lookup_string32(size_t, uint32_t) const;
#if ARCH(i386)
		const Elf32FileHeader& file_header_native() const { return file_header32(); }
		const Elf32ProgramHeader& program_header_native(size_t index) const { return program_header32(index); }
		const Elf32SectionHeader& section_header_native(size_t index) const { return section_header32(index); }
		const char* lookup_section_name_native(uint32_t offset) const { return lookup_section_name32(offset); }
		const char* lookup_string_native(size_t table_index, uint32_t offset) const { return lookup_string32(table_index, offset); }
		bool is_native() const { return is_x86_32(); }
#endif

		const uint8_t* data() const { return m_data.data(); }

		bool is_x86_32() const;
		bool is_x86_64() const;

	private:
//#ifdef __is_kernel
//		ELF(BAN::UniqPtr<Kernel::VirtualRange>&& storage, size_t size)
//			: m_storage(BAN::move(storage))
//			, m_data((const uint8_t*)m_storage->vaddr(), size)
//		{}
//#else
		ELF(BAN::Vector<uint8_t>&& data)
			: m_data(BAN::move(data))
		{}
//#endif
		BAN::ErrorOr<void> load();
		
		bool parse_elf64_file_header(const Elf64FileHeader&);
		bool parse_elf64_program_header(const Elf64ProgramHeader&);
		bool parse_elf64_section_header(const Elf64SectionHeader&);

		bool parse_elf32_file_header(const Elf32FileHeader&);
		bool parse_elf32_program_header(const Elf32ProgramHeader&);
		bool parse_elf32_section_header(const Elf32SectionHeader&);

	private:
//#ifdef __is_kernel
//		BAN::UniqPtr<Kernel::VirtualRange> m_storage;
//		BAN::Span<const uint8_t> m_data;
//#else
		const BAN::Vector<uint8_t> m_data;
//#endif
	};

}