#pragma once

#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include "Types.h"

namespace LibELF
{

	class ELF
	{
	public:
		static BAN::ErrorOr<ELF*> load_from_file(BAN::StringView);

		const Elf64FileHeader& file_header64() const;
		const Elf64ProgramHeader& program_header64(size_t) const;
		const Elf64SectionHeader& section_header64(size_t) const;

		const char* lookup_section_name(uint32_t) const;
		const char* lookup_string(size_t, uint32_t) const;

	private:
		ELF(BAN::Vector<uint8_t>&& data)
			: m_data(BAN::move(data))
		{}
		BAN::ErrorOr<void> load();
		
		bool parse_elf64_file_header(const Elf64FileHeader&);
		bool parse_elf64_section_header(const Elf64SectionHeader&);

	private:
		const BAN::Vector<uint8_t> m_data;
	};

}