#pragma once

#include <LibELF/Types.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <sys/stat.h>

class ELFFile
{
public:
	ELFFile(std::string_view path);
	~ELFFile();

	const LibELF::ElfNativeFileHeader& elf_header() const;
	std::optional<std::span<const uint8_t>> find_section(std::string_view name) const;

	bool success() const { return m_success; }

	std::string_view path() const { return m_path; }

private:
	const LibELF::ElfNativeSectionHeader& section_header(std::size_t index) const;
	std::string_view section_name(const LibELF::ElfNativeSectionHeader&) const;
	bool validate_elf_header() const;

private:
	const std::string	m_path;
	bool				m_success	{ false };
	int					m_fd		{ -1 };
	struct stat			m_stat		{ };
	uint8_t*			m_mmap		{ nullptr };
};
