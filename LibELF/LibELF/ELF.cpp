#include <BAN/ScopeGuard.h>
#include <kernel/Process.h>
#include <LibELF/ELF.h>
#include <LibELF/Values.h>

#include <fcntl.h>

namespace LibELF
{

	BAN::ErrorOr<ELF*> ELF::load_from_file(BAN::StringView file_path)
	{
		ELF* elf = nullptr;

		{
			BAN::Vector<uint8_t> data;

			int fd = TRY(Kernel::Process::current()->open(file_path, O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Kernel::Process::current()->close(fd)); });

			stat st;
			TRY(Kernel::Process::current()->fstat(fd, &st));

			TRY(data.resize(st.st_size));

			TRY(Kernel::Process::current()->read(fd, data.data(), data.size()));

			elf = new ELF(BAN::move(data));
			ASSERT(elf);
		}

		if (auto res = elf->load(); res.is_error())
		{
			delete elf;
			return res.error();
		}

		return elf;
	}

	const char* ELF::lookup_section_name(uint32_t offset) const
	{
		return lookup_string(file_header64().e_shstrndx, offset);
	}

	const char* ELF::lookup_string(size_t table_index, uint32_t offset) const
	{
		if (table_index == SHN_UNDEF)
			return nullptr;
		auto& section_header = section_header64(table_index);
		return (const char*)m_data.data() + section_header.sh_offset + offset;
	}

	bool ELF::parse_elf64_file_header(const Elf64FileHeader& header)
	{
		if (header.e_type != ET_EXEC)
		{
			dprintln("Only executable files are supported");
			return false;
		}

		if (header.e_version != EV_CURRENT)
		{
			dprintln("Invalid ELF version");
			return false;
		}

		return true;
	}

	bool ELF::parse_elf64_section_header(const Elf64SectionHeader& header)
	{
		if (auto* name = lookup_section_name(header.sh_name))
			dprintln("{}", name);

		switch (header.sh_type)
		{
			case SHT_NULL:
				dprintln("  SHT_NULL");
				break;
			case SHT_PROGBITS:
				dprintln("  SHT_PROGBITS");
				break;
			case SHT_SYMTAB:
				for (size_t i = 1; i < header.sh_size / header.sh_entsize; i++)
				{
					auto& symbol = ((const Elf64Symbol*)(m_data.data() + header.sh_offset))[i];
					if (auto* name = lookup_string(header.sh_link, symbol.st_name))
						dprintln("  {}", name);
				}
				break;
			case SHT_STRTAB:
				dprintln("  SHT_STRTAB");
				break;
			case SHT_RELA:
				dprintln("  SHT_RELA");
				break;
			case SHT_NOBITS:
				dprintln("  SHT_NOBITS");
				break;
			case SHT_REL:
				dprintln("  SHT_REL");
				break;
			case SHT_SHLIB:
				dprintln("  SHT_SHLIB");
				break;
			case SHT_DYNSYM:
				dprintln("  SHT_DYNSYM");
				break;
			default:
				ASSERT(false);
		}	

		return true;
	}

	BAN::ErrorOr<void> ELF::load()
	{
		if (m_data.size() < EI_NIDENT)
		{
			dprintln("Too small ELF file");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_data[EI_MAG0] != ELFMAG0 || 
			m_data[EI_MAG1] != ELFMAG1 ||
			m_data[EI_MAG2] != ELFMAG2 ||
			m_data[EI_MAG3] != ELFMAG3)
		{
			dprintln("Invalid ELF header");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_data[EI_DATA] != ELFDATA2LSB)
		{
			dprintln("Only little-endian is supported");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_data[EI_VERSION] != EV_CURRENT)
		{
			dprintln("Invalid ELF version");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_data[EI_CLASS] != ELFCLASS64)
		{
			dprintln("Only 64 bit is supported");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_data.size() <= sizeof(Elf64FileHeader))
		{
			dprintln("Too small ELF file");
			return BAN::Error::from_errno(EINVAL);
		}

		auto& header = file_header64();
		if (!parse_elf64_file_header(header))
			return BAN::Error::from_errno(EINVAL);

		for (size_t i = 1; i < header.e_shnum; i++)
		{
			auto& section_header = section_header64(i);
			if (!parse_elf64_section_header(section_header))
				return BAN::Error::from_errno(EINVAL);
		}

		return {};
	}


	const Elf64FileHeader& ELF::file_header64() const
	{
		return *(const Elf64FileHeader*)m_data.data();
	}

	const Elf64ProgramHeader& ELF::program_header64(size_t index) const
	{
		return ((const Elf64ProgramHeader*)(m_data.data() + file_header64().e_phoff))[index];
	}

	const Elf64SectionHeader& ELF::section_header64(size_t index) const
	{
		return ((const Elf64SectionHeader*)(m_data.data() + file_header64().e_shoff))[index];
	}


}
