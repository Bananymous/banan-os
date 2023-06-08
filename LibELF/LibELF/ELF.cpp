#include <BAN/ScopeGuard.h>
#include <LibELF/ELF.h>
#include <LibELF/Values.h>

#ifdef __is_kernel
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Memory/PageTableScope.h>
#include <kernel/Process.h>
#endif

#include <fcntl.h>

#define ELF_PRINT_HEADERS 0

#ifdef __is_kernel
extern uint8_t g_kernel_end[];
using namespace Kernel;
#endif

namespace LibELF
{

#ifdef __is_kernel
	BAN::ErrorOr<BAN::UniqPtr<ELF>> ELF::load_from_file(BAN::StringView file_path)
	{
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(file_path, true));

		PageTable::current().lock();
		size_t page_count = BAN::Math::div_round_up<size_t>(file.inode->size(), PAGE_SIZE);
		vaddr_t vaddr = PageTable::current().get_free_contiguous_pages(page_count, (vaddr_t)g_kernel_end);
		auto virtual_range = BAN::UniqPtr<VirtualRange>::adopt(
			VirtualRange::create(
				PageTable::current(),
				vaddr, page_count * PAGE_SIZE,
				PageTable::Flags::ReadWrite | PageTable::Flags::Present
			)
		);
		PageTable::current().unlock();

		TRY(file.inode->read(0, (void*)vaddr, file.inode->size()));

		ELF* elf_ptr = new ELF(BAN::move(virtual_range), file.inode->size());
		if (elf_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		auto elf = BAN::UniqPtr<ELF>::adopt(elf_ptr);
		TRY(elf->load());

		return BAN::move(elf);
	}
#else
	BAN::ErrorOr<ELF*> ELF::load_from_file(BAN::StringView file_path)
	{
		ELF* elf = nullptr;

		{
			BAN::Vector<uint8_t> data;

			int fd = TRY(Kernel::Process::current().open(file_path, O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Kernel::Process::current().close(fd)); });

			struct stat st;
			TRY(Kernel::Process::current().fstat(fd, &st));

			TRY(data.resize(st.st_size));

			TRY(Kernel::Process::current().read(fd, data.data(), data.size()));

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
#endif

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

		if (m_data[EI_CLASS] == ELFCLASS64)
		{
			if (m_data.size() <= sizeof(Elf64FileHeader))
			{
				dprintln("Too small ELF file");
				return BAN::Error::from_errno(EINVAL);
			}

			auto& header = file_header64();
			if (!parse_elf64_file_header(header))
				return BAN::Error::from_errno(EINVAL);

			for (size_t i = 0; i < header.e_phnum; i++)
			{
				auto& program_header = program_header64(i);
				if (!parse_elf64_program_header(program_header))
					return BAN::Error::from_errno(EINVAL);
			}

			for (size_t i = 1; i < header.e_shnum; i++)
			{
				auto& section_header = section_header64(i);
				if (!parse_elf64_section_header(section_header))
					return BAN::Error::from_errno(EINVAL);
			}
		}
		else if (m_data[EI_CLASS] == ELFCLASS32)
		{
			if (m_data.size() <= sizeof(Elf32FileHeader))
			{
				dprintln("Too small ELF file");
				return BAN::Error::from_errno(EINVAL);
			}

			auto& header = file_header32();
			if (!parse_elf32_file_header(header))
				return BAN::Error::from_errno(EINVAL);

			for (size_t i = 0; i < header.e_phnum; i++)
			{
				auto& program_header = program_header32(i);
				if (!parse_elf32_program_header(program_header))
					return BAN::Error::from_errno(EINVAL);
			}

			for (size_t i = 1; i < header.e_shnum; i++)
			{
				auto& section_header = section_header32(i);
				if (!parse_elf32_section_header(section_header))
					return BAN::Error::from_errno(EINVAL);
			}
		}

		return {};
	}

	bool ELF::is_x86_32() const { return m_data[EI_CLASS] == ELFCLASS32; }
	bool ELF::is_x86_64() const { return m_data[EI_CLASS] == ELFCLASS64; }

	/*
	
		64 bit ELF

	*/

	const char* ELF::lookup_section_name64(uint32_t offset) const
	{
		return lookup_string64(file_header64().e_shstrndx, offset);
	}

	const char* ELF::lookup_string64(size_t table_index, uint32_t offset) const
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

	bool ELF::parse_elf64_program_header(const Elf64ProgramHeader& header)
	{
#if ELF_PRINT_HEADERS
		dprintln("program header");
		dprintln("  type   {H}", header.p_type);
		dprintln("  flags  {H}", header.p_flags);
		dprintln("  offset {H}", header.p_offset);
		dprintln("  vaddr  {H}", header.p_vaddr);
		dprintln("  paddr  {H}", header.p_paddr);
		dprintln("  filesz {}", header.p_filesz);
		dprintln("  memsz  {}", header.p_memsz);
		dprintln("  align  {}", header.p_align);
#endif
		(void)header;
		return true;
	}

	bool ELF::parse_elf64_section_header(const Elf64SectionHeader& header)
	{
#if ELF_PRINT_HEADERS
		if (auto* name = lookup_section_name64(header.sh_name))
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
					if (auto* name = lookup_string64(header.sh_link, symbol.st_name))
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
#endif
		(void)header;
		return true;
	}

	const Elf64FileHeader& ELF::file_header64() const
	{
		ASSERT(is_x86_64());
		return *(const Elf64FileHeader*)m_data.data();
	}

	const Elf64ProgramHeader& ELF::program_header64(size_t index) const
	{
		ASSERT(is_x86_64());
		const auto& file_header = file_header64();
		ASSERT(index < file_header.e_phnum);
		return *(const Elf64ProgramHeader*)(m_data.data() + file_header.e_phoff + file_header.e_phentsize * index);
	}

	const Elf64SectionHeader& ELF::section_header64(size_t index) const
	{
		ASSERT(is_x86_64());
		const auto& file_header = file_header64();
		ASSERT(index < file_header.e_shnum);
		return *(const Elf64SectionHeader*)(m_data.data() + file_header.e_shoff + file_header.e_shentsize * index);
	}


	/*
	
		32 bit ELF

	*/

	const char* ELF::lookup_section_name32(uint32_t offset) const
	{
		return lookup_string32(file_header32().e_shstrndx, offset);
	}

	const char* ELF::lookup_string32(size_t table_index, uint32_t offset) const
	{
		if (table_index == SHN_UNDEF)
			return nullptr;
		auto& section_header = section_header32(table_index);
		return (const char*)m_data.data() + section_header.sh_offset + offset;
	}

	bool ELF::parse_elf32_file_header(const Elf32FileHeader& header)
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

	bool ELF::parse_elf32_program_header(const Elf32ProgramHeader& header)
	{
#if ELF_PRINT_HEADERS
		dprintln("program header");
		dprintln("  type   {H}", header.p_type);
		dprintln("  flags  {H}", header.p_flags);
		dprintln("  offset {H}", header.p_offset);
		dprintln("  vaddr  {H}", header.p_vaddr);
		dprintln("  paddr  {H}", header.p_paddr);
		dprintln("  filesz {}", header.p_filesz);
		dprintln("  memsz  {}", header.p_memsz);
		dprintln("  align  {}", header.p_align);
#endif
		(void)header;
		return true;
	}

	bool ELF::parse_elf32_section_header(const Elf32SectionHeader& header)
	{
#if ELF_PRINT_HEADERS
		if (auto* name = lookup_section_name32(header.sh_name))
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
					auto& symbol = ((const Elf32Symbol*)(m_data.data() + header.sh_offset))[i];
					if (auto* name = lookup_string32(header.sh_link, symbol.st_name))
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
#endif
		(void)header;
		return true;
	}

	const Elf32FileHeader& ELF::file_header32() const
	{
		ASSERT(is_x86_32());
		return *(const Elf32FileHeader*)m_data.data();
	}

	const Elf32ProgramHeader& ELF::program_header32(size_t index) const
	{
		ASSERT(is_x86_32());
		const auto& file_header = file_header32();
		ASSERT(index < file_header.e_phnum);
		return *(const Elf32ProgramHeader*)(m_data.data() + file_header.e_phoff + file_header.e_phentsize * index);
	}

	const Elf32SectionHeader& ELF::section_header32(size_t index) const
	{
		ASSERT(is_x86_32());
		const auto& file_header = file_header32();
		ASSERT(index < file_header.e_shnum);
		return *(const Elf32SectionHeader*)(m_data.data() + file_header.e_shoff + file_header.e_shentsize * index);
	}


}
