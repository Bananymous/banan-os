#include "ELF.h"

#include <LibELF/Values.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

using namespace LibELF;

ELFFile::ELFFile(std::string_view path)
	: m_path(path)
{
	m_fd = open(m_path.c_str(), O_RDONLY);
	if (m_fd == -1)
	{
		std::cerr << "Could not open '" << m_path << "': " << std::strerror(errno) << std::endl;
		return;
	}

	if (fstat(m_fd, &m_stat) == -1)
	{
		std::cerr << "Could not stat '" << m_path << "': " << std::strerror(errno) << std::endl;
		return;
	}

	void* mmap_addr = mmap(nullptr, m_stat.st_size, PROT_READ, MAP_PRIVATE, m_fd, 0);
	if (mmap_addr == MAP_FAILED)
	{
		std::cerr << "Could not mmap '" << m_path << "': " << std::strerror(errno) << std::endl;
		return;
	}
	m_mmap = reinterpret_cast<uint8_t*>(mmap_addr);

	if (!validate_elf_header())
		return;

	m_success = true;
}

ELFFile::~ELFFile()
{
	if (m_mmap)
		munmap(m_mmap, m_stat.st_size);
	m_mmap = nullptr;

	if (m_fd != -1)
		close(m_fd);
	m_fd = -1;
}

const ElfNativeFileHeader& ELFFile::elf_header() const
{
	return *reinterpret_cast<LibELF::ElfNativeFileHeader*>(m_mmap);
}

bool ELFFile::validate_elf_header() const
{
	if (m_stat.st_size < sizeof(ElfNativeFileHeader))
	{
		std::cerr << m_path << " is too small to be a ELF executable" << std::endl;
		return false;
	}

	const auto& elf_header = this->elf_header();

	if (
		elf_header.e_ident[EI_MAG0] != ELFMAG0 ||
		elf_header.e_ident[EI_MAG1] != ELFMAG1 ||
		elf_header.e_ident[EI_MAG2] != ELFMAG2 ||
		elf_header.e_ident[EI_MAG3] != ELFMAG3
	)
	{
		std::cerr << m_path << " doesn't have an ELF magic number" << std::endl;
		return false;
	}

#if ARCH(x86_64)
	if (elf_header.e_ident[EI_CLASS] != ELFCLASS64)
#elif ARCH(i386)
	if (elf_header.e_ident[EI_CLASS] != ELFCLASS32)
#endif
	{
		std::cerr << m_path << " architecture doesn't match" << std::endl;
		return false;
	}

	if (elf_header.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		std::cerr << m_path << " is not in little endian format" << std::endl;
		return false;
	}

	if (elf_header.e_ident[EI_VERSION] != EV_CURRENT)
	{
		std::cerr << m_path << " has unsupported version" << std::endl;
		return false;
	}

	if (elf_header.e_type != ET_EXEC)
	{
		std::cerr << m_path << " is not an executable ELF file" << std::endl;
		return false;
	}

	return true;
}

const ElfNativeSectionHeader& ELFFile::section_header(std::size_t index) const
{
	const auto& elf_header = this->elf_header();
	assert(index < elf_header.e_shnum);
	const uint8_t* section_array_start = m_mmap + elf_header.e_shoff;
	return *reinterpret_cast<const ElfNativeSectionHeader*>(section_array_start + index * elf_header.e_shentsize);
}

std::string_view ELFFile::section_name(const ElfNativeSectionHeader& section_header) const
{
	const auto& elf_header = this->elf_header();
	assert(elf_header.e_shstrndx != SHN_UNDEF);
	const auto& section_string_table = this->section_header(elf_header.e_shstrndx);
	const char* string_table_start = reinterpret_cast<const char*>(m_mmap + section_string_table.sh_offset);
	return string_table_start + section_header.sh_name;
}

std::optional<std::span<const uint8_t>> ELFFile::find_section(std::string_view name) const
{
	const auto& elf_header = this->elf_header();
	for (std::size_t i = 0; i < elf_header.e_shnum; i++)
	{
		const auto& section_header = this->section_header(i);
		auto section_name = this->section_name(section_header);
		if (section_name != name)
			continue;
		return std::span<const uint8_t>(m_mmap + section_header.sh_offset, section_header.sh_size);
	}
	return {};
}
