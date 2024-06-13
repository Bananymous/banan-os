#pragma once

#include <BAN/String.h>

namespace Kernel::FAT
{

	struct ExtBPB_12_16
	{
		uint8_t drive_number;
		uint8_t __reserved0;
		uint8_t boot_signature;
		uint32_t volume_id;
		uint8_t volume_label[11];
		uint8_t filesystem_type[8];
		uint8_t __reserved1[448];
	} __attribute__((packed));
	static_assert(sizeof(ExtBPB_12_16) == 510 - 36);

	struct ExtBPB_32
	{
		uint32_t fat_size32;
		uint16_t extended_flags;
		uint16_t filesystem_version;
		uint32_t root_cluster;
		uint16_t filesystem_info;
		uint16_t backup_boot_sector;
		uint8_t __reserved0[12];
		uint8_t drive_number;
		uint8_t __reserved1;
		uint8_t boot_signature;
		uint32_t volume_id;
		uint8_t volume_label[11];
		uint8_t filesystem_type[8];
		uint8_t __reserved2[420];
	} __attribute__((packed));
	static_assert(sizeof(ExtBPB_32) == 510 - 36);

	struct BPB
	{
		uint8_t jump_op[3];
		uint8_t oem_name[8];
		uint16_t bytes_per_sector;
		uint8_t sectors_per_cluster;
		uint16_t reserved_sector_count;
		uint8_t number_of_fats;
		uint16_t root_entry_count;
		uint16_t total_sectors16;
		uint8_t media_type;
		uint16_t fat_size16;
		uint16_t sectors_per_track;
		uint16_t number_of_heads;
		uint32_t hidden_sector_count;
		uint32_t total_sectors32;
		union
		{
			ExtBPB_12_16 ext_12_16;
			ExtBPB_32 ext_32;
		};
		uint16_t Signature_word;
	} __attribute__((packed));
	static_assert(sizeof(BPB) == 512);

	struct Date
	{
		uint16_t day   : 5;
		uint16_t month : 4;
		uint16_t year  : 7;
	};

	struct Time
	{
		uint16_t second : 5;
		uint16_t minute : 6;
		uint16_t hour   : 5;
	};

	struct DirectoryEntry
	{
		uint8_t  name[11];
		uint8_t  attr;
		uint8_t  ntres;
		uint8_t  creation_time_hundreth;
		Time     creation_time;
		Date     creation_date;
		Date     last_access_date;
		uint16_t first_cluster_hi;
		Time     write_time;
		Date     write_date;
		uint16_t first_cluster_lo;
		uint32_t file_size;

		BAN::String name_as_string() const
		{
			static_assert(BAN::String::sso_capacity >= 8 + 3 + 1);

			BAN::String short_name;
			MUST(short_name.append(BAN::StringView((const char*)&name[0], 8)));
			while (short_name.back() == ' ')
				short_name.pop_back();
			MUST(short_name.push_back('.'));
			MUST(short_name.append(BAN::StringView((const char*)&name[8], 3)));
			while (short_name.back() == ' ')
				short_name.pop_back();
			if (short_name.back() == '.')
				short_name.pop_back();
			return short_name;
		}
	} __attribute__((packed));
	static_assert(sizeof(DirectoryEntry) == 32);

	struct LongNameEntry
	{
		uint8_t order;
		uint16_t name1[5];
		uint8_t attr;
		uint8_t type;
		uint8_t checksum;
		uint16_t name2[6];
		uint16_t first_cluster_lo;
		uint16_t name3[2];

		BAN::String name_as_string() const
		{
			static_assert(BAN::String::sso_capacity >= 13);

			BAN::String result;
			for (uint16_t ch : name1) {
				if (ch == 0)
					return result;
				MUST(result.push_back(ch));
			}
			for (uint16_t ch : name2) {
				if (ch == 0)
					return result;
				MUST(result.push_back(ch));
			}
			for (uint16_t ch : name3) {
				if (ch == 0)
					return result;
				MUST(result.push_back(ch));
			}
			return result;
		}
	} __attribute__((packed));
	static_assert(sizeof(LongNameEntry) == 32);

	enum FileAttr : uint8_t
	{
		READ_ONLY = 0x01,
		HIDDEN = 0x02,
		SYSTEM = 0x04,
		VOLUME_ID = 0x08,
		DIRECTORY = 0x10,
		ARCHIVE = 0x20,
	};

}
