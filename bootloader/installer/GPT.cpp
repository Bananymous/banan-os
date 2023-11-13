#include "crc32.h"
#include "GPT.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

// FIXME: don't assume 512 byte sectors
#define SECTOR_SIZE 512

GPTFile::GPTFile(std::string_view path)
	: m_path(path)
{
	m_fd = open(m_path.c_str(), O_RDWR);
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

	void* mmap_addr = mmap(nullptr, m_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
	if (mmap_addr == MAP_FAILED)
	{
		std::cerr << "Could not mmap '" << m_path << "': " << std::strerror(errno) << std::endl;
		return;
	}
	m_mmap = reinterpret_cast<uint8_t*>(mmap_addr);

	if (!validate_gpt_header())
		return;

	m_success = true;
}

GPTFile::~GPTFile()
{
	if (m_mmap)
		munmap(m_mmap, m_stat.st_size);
	m_mmap = nullptr;

	if (m_fd != -1)
		close(m_fd);
	m_fd = -1;
}

MBR& GPTFile::mbr()
{
	return *reinterpret_cast<MBR*>(m_mmap);
}

const GPTHeader& GPTFile::gpt_header() const
{
    return *reinterpret_cast<GPTHeader*>(m_mmap + SECTOR_SIZE);
}

bool GPTFile::install_stage1(std::span<const uint8_t> stage1)
{
	auto& mbr = this->mbr();

	if (stage1.size() > sizeof(mbr.boot_code))
	{
		std::cerr << m_path << ": can't fit " << stage1.size() << " bytes of boot code in mbr (max is " << sizeof(mbr.boot_code) << ")" << std::endl;
		return false;
	}

	// copy boot code
	memcpy(mbr.boot_code, stage1.data(), stage1.size());

	// setup mbr
	mbr.unique_mbr_disk_signature = 0xdeadbeef;
	mbr.unknown = 0;
	mbr.signature = 0xAA55;

	// setup mbr partition records
	mbr.partition_records[0].boot_indicator = 0x00;
	mbr.partition_records[0].starting_chs[0] = 0x00;
	mbr.partition_records[0].starting_chs[1] = 0x02;
	mbr.partition_records[0].starting_chs[2] = 0x00;
	mbr.partition_records[0].os_type = 0xEE;
	mbr.partition_records[0].ending_chs[0] = 0xFF;
	mbr.partition_records[0].ending_chs[1] = 0xFF;
	mbr.partition_records[0].ending_chs[2] = 0xFF;
	mbr.partition_records[0].starting_lba = 1;
	mbr.partition_records[0].size_in_lba = 0xFFFFFFFF;
	memset(&mbr.partition_records[1], 0x00, sizeof(MBRPartitionRecord));
	memset(&mbr.partition_records[2], 0x00, sizeof(MBRPartitionRecord));
	memset(&mbr.partition_records[3], 0x00, sizeof(MBRPartitionRecord));

	return true;
}

bool GPTFile::install_stage2(std::span<const uint8_t> stage2, const GUID& root_partition_guid)
{
	if (stage2.size() < 16)
	{
		std::cerr << m_path << ": contains invalid .stage2 section, too small for patches" << std::endl;
		return false;
	}

	// find GUID patch offsets
	std::size_t disk_guid_offset(-1);
	std::size_t part_guid_offset(-1);
	for (std::size_t i = 0; i < stage2.size() - 16; i++)
	{
		if (memcmp(stage2.data() + i, "root disk guid  ", 16) == 0)
		{
			if (disk_guid_offset != std::size_t(-1))
			{
				std::cerr << m_path << ": contains invalid .stage2 section, multiple patchable disk guids" << std::endl;
				return false;
			}
			disk_guid_offset = i;
		}
		if (memcmp(stage2.data() + i, "root part guid  ", 16) == 0)
		{
			if (part_guid_offset != std::size_t(-1))
			{
				std::cerr << m_path << ": contains invalid .stage2 section, multiple patchable partition guids" << std::endl;
				return false;
			}
			part_guid_offset = i;
		}
	}
	if (disk_guid_offset == std::size_t(-1))
	{
		std::cerr << m_path << ": contains invalid .stage2 section, no patchable disk guid" << std::endl;
		return false;
	}
	if (part_guid_offset == std::size_t(-1))
	{
		std::cerr << m_path << ": contains invalid .stage2 section, no patchable partition guid" << std::endl;
		return false;
	}
	

	auto partition = find_partition_with_type(bios_boot_guid);
	if (!partition.has_value())
	{
		std::cerr << m_path << ": could not find partition with type " << bios_boot_guid << std::endl;
		return false;
	}

	const std::size_t partition_size = (partition->ending_lba - partition->starting_lba + 1) * SECTOR_SIZE;

	if (stage2.size() > partition_size)
	{
		std::cerr << m_path << ": can't fit " << stage2.size() << " bytes of data to partition of size " << partition_size << std::endl;
		return false;
	}

	uint8_t* partition_start = m_mmap + partition->starting_lba * SECTOR_SIZE;
	memcpy(partition_start, stage2.data(), stage2.size());

	// patch GUIDs
	*reinterpret_cast<GUID*>(partition_start + disk_guid_offset) = gpt_header().disk_guid;
	*reinterpret_cast<GUID*>(partition_start + part_guid_offset) = root_partition_guid;

	return true;
}

bool GPTFile::install_bootloader(std::span<const uint8_t> stage1, std::span<const uint8_t> stage2, const GUID& root_partition_guid)
{
	if (!find_partition_with_guid(root_partition_guid).has_value())
	{
		std::cerr << m_path << ": no partition with GUID " << root_partition_guid << std::endl;
		return false;
	}
	if (!install_stage1(stage1))
		return false;
	if (!install_stage2(stage2, root_partition_guid))
		return false;
	return true;
}

std::optional<GPTPartitionEntry> GPTFile::find_partition_with_guid(const GUID& guid) const
{
	const auto& gpt_header = this->gpt_header();
	const uint8_t* partition_entry_array_start = m_mmap + gpt_header.partition_entry_lba * SECTOR_SIZE;
	for (std::size_t i = 0; i < gpt_header.number_of_partition_entries; i++)
	{
		const auto& partition_entry = *reinterpret_cast<const GPTPartitionEntry*>(partition_entry_array_start + i * gpt_header.size_of_partition_entry);
		if (partition_entry.partition_guid != guid)
			continue;
		return partition_entry;
	}
	return {};
}

std::optional<GPTPartitionEntry> GPTFile::find_partition_with_type(const GUID& type_guid) const
{
	const auto& gpt_header = this->gpt_header();
	const uint8_t* partition_entry_array_start = m_mmap + gpt_header.partition_entry_lba * SECTOR_SIZE;
	for (std::size_t i = 0; i < gpt_header.number_of_partition_entries; i++)
	{
		const auto& partition_entry = *reinterpret_cast<const GPTPartitionEntry*>(partition_entry_array_start + i * gpt_header.size_of_partition_entry);
		if (partition_entry.type_guid != type_guid)
			continue;
		return partition_entry;
	}
	return {};
}

bool GPTFile::validate_gpt_header() const
{
    if (SECTOR_SIZE + m_stat.st_size < sizeof(GPTHeader))
    {
		std::cerr << m_path << " is too small to have GPT header" << std::endl;
        return false;
    }

    auto gpt_header = this->gpt_header();

    if (std::memcmp(gpt_header.signature, "EFI PART", 8) != 0)
    {
		std::cerr << m_path << " doesn't contain GPT partition header signature" << std::endl;
		return false;
    }

	const uint32_t header_crc32 = gpt_header.header_crc32;

	gpt_header.header_crc32 = 0;
	if (header_crc32 != crc32_checksum(reinterpret_cast<uint8_t*>(&gpt_header), gpt_header.header_size))
	{
		std::cerr << m_path << " has non-matching header crc32" << std::endl;
		return false;
	}

	const std::size_t partition_array_size = gpt_header.number_of_partition_entries * gpt_header.size_of_partition_entry;
	if (gpt_header.partition_entry_array_crc32 != crc32_checksum(m_mmap + gpt_header.partition_entry_lba * SECTOR_SIZE, partition_array_size))
	{
		std::cerr << m_path << " has non-matching partition entry crc32" << std::endl;
		return false;
	}

    return true;
}
