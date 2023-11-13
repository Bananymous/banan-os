#pragma once

#include "GUID.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <sys/stat.h>

struct MBRPartitionRecord
{
	uint8_t boot_indicator;
	uint8_t starting_chs[3];
	uint8_t os_type;
	uint8_t ending_chs[3];
	uint32_t starting_lba;
	uint32_t size_in_lba;
} __attribute__((packed));

struct MBR
{
	uint8_t boot_code[440];
	uint32_t unique_mbr_disk_signature;
	uint16_t unknown;
	MBRPartitionRecord partition_records[4];
	uint16_t signature;
} __attribute__((packed));
static_assert(sizeof(MBR) == 512);

struct GPTPartitionEntry
{
	GUID type_guid;
	GUID partition_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	uint64_t attributes;
	uint16_t name[36];
};
static_assert(sizeof(GPTPartitionEntry) == 128);

struct GPTHeader
{
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	GUID disk_guid;
	uint64_t partition_entry_lba;
	uint32_t number_of_partition_entries;
	uint32_t size_of_partition_entry;
	uint32_t partition_entry_array_crc32;
} __attribute__((packed));
static_assert(sizeof(GPTHeader) == 92);

class GPTFile
{
public:
	GPTFile(std::string_view path);
	~GPTFile();

	bool install_bootloader(std::span<const uint8_t> stage1, std::span<const uint8_t> stage2, const GUID& root_partition_guid);

	const GPTHeader& gpt_header() const;

	bool success() const { return m_success; }

	std::string_view path() const { return m_path; }

private:
	MBR& mbr();
	bool validate_gpt_header() const;
	std::optional<GPTPartitionEntry> find_partition_with_guid(const GUID& guid) const;
	std::optional<GPTPartitionEntry> find_partition_with_type(const GUID& type_guid) const;

	bool install_stage1(std::span<const uint8_t> stage1);
	bool install_stage2(std::span<const uint8_t> stage2, const GUID& root_partition_guid);

private:
	const std::string	m_path;
	bool				m_success	{ false };
	int					m_fd		{ -1 };
	struct stat			m_stat		{ };
	uint8_t*			m_mmap		{ nullptr };
};
