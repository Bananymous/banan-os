#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ATA.h>
#include <kernel/FS/Ext2.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/DiskIO.h>

#include <kernel/kprint.h>

#define ATA_DEVICE_PRIMARY		0x1F0
#define ATA_DEVICE_SECONDARY	0x170
#define ATA_DEVICE_SLAVE_BIT	0x10

namespace Kernel
{

	struct GPTHeader
	{
		char signature[8];
		uint32_t revision;
		uint32_t size;
		uint32_t crc32;
		uint64_t my_lba;
		uint64_t first_lba;
		uint64_t last_lba;
		GUID     guid;
		uint64_t partition_entry_lba;
		uint32_t partition_entry_count;
		uint32_t partition_entry_size;
		uint32_t partition_entry_array_crc32;
	};

	uint32_t crc32_table[256] =
	{
		0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
		0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
		0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
		0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
		0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
		0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
		0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
		0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
		0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
		0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
		0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
		0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
		0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
		0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
		0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
		0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
		0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
		0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
		0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
		0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
		0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
		0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
		0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
		0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
		0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
		0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
		0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
		0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
		0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
		0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
		0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
		0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
		0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
		0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
		0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
		0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
		0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
		0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
		0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
		0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
		0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
		0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
		0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
		0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
		0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
		0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
		0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
		0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
		0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
		0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
		0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
		0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
		0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
		0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
		0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
		0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
		0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
		0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
		0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
		0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
		0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
		0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
		0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
		0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
	};

	static uint32_t crc32_checksum(const uint8_t* data, size_t count)
	{
		uint32_t crc32 = 0xFFFFFFFF;
		for (size_t i = 0; i < count; i++)
		{
			uint8_t index = (crc32 ^ data[i]) & 0xFF;
			crc32 = (crc32 >> 8) ^ crc32_table[index];
		}
		return crc32 ^ 0xFFFFFFFF;
	}

	template<typename T>
	static T little_endian_to_host(const uint8_t* data)
	{
		T result = 0;
		for (size_t i = 0; i < sizeof(T); i++)
			result |= data[i] << (8 * i);
		return result;
	}

	template<typename T>
	static T big_endian_to_host(const uint8_t* data)
	{
		T result = 0;
		for (size_t i = 0; i < sizeof(T); i++)
			result |= data[i] << (8 * (sizeof(T) - i - 1));
		return result;
	}

	static GUID parse_guid(const uint8_t* guid)
	{
		GUID result;
		result.data1 = big_endian_to_host<uint32_t>(guid + 0);
		result.data2 = big_endian_to_host<uint16_t>(guid + 4);
		result.data3 = big_endian_to_host<uint16_t>(guid + 6);
		memcpy(result.data4, guid + 8, 8);
		return result;
	}

	static bool is_valid_gpt_header(const GPTHeader& header, uint32_t sector_size)
	{
		if (memcmp(header.signature, "EFI PART", 8) != 0)
			return false;
		if (header.revision != 0x00010000)
			return false;
		if (header.size < 92 || header.size > sector_size)
			return false;
		if (header.my_lba != 1)
			return false;
		return true;
	}

	static bool is_valid_gpt_crc32(const GPTHeader& header, BAN::Vector<uint8_t> lba1, const BAN::Vector<uint8_t>& entry_array)
	{
		memset(lba1.data() + 16, 0, 4);
		if (header.crc32 != crc32_checksum(lba1.data(), header.size))
			return false;
		if (header.partition_entry_array_crc32 != crc32_checksum(entry_array.data(), header.partition_entry_count * header.partition_entry_size))
			return false;
		return true;
	}

	static GPTHeader parse_gpt_header(const BAN::Vector<uint8_t>& lba1)
	{
		GPTHeader header;
		memset(&header, 0, sizeof(header));

		memcpy(header.signature, lba1.data(), 8);
		header.revision						= little_endian_to_host<uint32_t>(lba1.data() + 8);
		header.size							= little_endian_to_host<uint32_t>(lba1.data() + 12);
		header.crc32						= little_endian_to_host<uint32_t>(lba1.data() + 16);
		header.my_lba						= little_endian_to_host<uint64_t>(lba1.data() + 24);
		header.first_lba					= little_endian_to_host<uint64_t>(lba1.data() + 40);
		header.last_lba						= little_endian_to_host<uint64_t>(lba1.data() + 48);
		header.guid							= parse_guid(lba1.data() + 56);
		header.partition_entry_lba			= little_endian_to_host<uint64_t>(lba1.data() + 72);
		header.partition_entry_count		= little_endian_to_host<uint32_t>(lba1.data() + 80);
		header.partition_entry_size			= little_endian_to_host<uint32_t>(lba1.data() + 84);
		header.partition_entry_array_crc32	= little_endian_to_host<uint32_t>(lba1.data() + 88);
		return header;
	}

	bool DiskDevice::initialize_partitions()
	{
		BAN::Vector<uint8_t> lba1(sector_size());
		if (!read_sectors(1, 1, lba1.data()))
			return false;

		GPTHeader header = parse_gpt_header(lba1);
		if (!is_valid_gpt_header(header, sector_size()))
		{
			dprintln("invalid gpt header");
			return false;
		}

		uint32_t size = header.partition_entry_count * header.partition_entry_size;
		if (uint32_t remainder = size % sector_size())
			size += sector_size() - remainder;

		BAN::Vector<uint8_t> entry_array(size);
		if (!read_sectors(header.partition_entry_lba, size / sector_size(), entry_array.data()))
			return false;

		if (!is_valid_gpt_crc32(header, lba1, entry_array))
		{
			dprintln("invalid crc3 in gpt header");
			return false;
		}

		for (uint32_t i = 0; i < header.partition_entry_count; i++)
		{
			uint8_t* partition_data = entry_array.data() + header.partition_entry_size * i;
			MUST(m_partitions.emplace_back(
				*this, 
				parse_guid(partition_data + 0),
				parse_guid(partition_data + 16),
				little_endian_to_host<uint64_t>(partition_data + 32),
				little_endian_to_host<uint64_t>(partition_data + 40),
				little_endian_to_host<uint64_t>(partition_data + 48),
				(const char*)(partition_data + 56)
			));
		}

		return true;
	}

	static DiskIO* s_instance = nullptr;

	bool DiskIO::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new DiskIO();

#if 1
		for (DiskDevice* device : s_instance->m_devices)
		{
			for (auto& partition : device->partitions())
			{
				if (!partition.is_used())
					continue;
				
				if (memcmp(&partition.type(), "\x0F\xC6\x3D\xAF\x84\x83\x47\x72\x8E\x79\x3D\x69\xD8\x47\x7D\xE4", 16) == 0)
				{
					auto ext2fs = MUST(Ext2FS::create(partition));
					VirtualFileSystem::initialize(ext2fs->root_inode());
				}
			}
		}
#endif

		return true;
	}

	DiskIO& DiskIO::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	DiskIO::DiskIO()
	{
		try_add_device(ATADevice::create(ATA_DEVICE_PRIMARY,   ATA_DEVICE_PRIMARY   + 0x206, 0));
		try_add_device(ATADevice::create(ATA_DEVICE_PRIMARY,   ATA_DEVICE_PRIMARY   + 0x206, ATA_DEVICE_SLAVE_BIT));
		try_add_device(ATADevice::create(ATA_DEVICE_SECONDARY, ATA_DEVICE_SECONDARY + 0x206, 0));
		try_add_device(ATADevice::create(ATA_DEVICE_SECONDARY, ATA_DEVICE_SECONDARY + 0x206, ATA_DEVICE_SLAVE_BIT));
	}

	void DiskIO::try_add_device(DiskDevice* device)
	{
		if (!device)
			return;
		if (!device->initialize())
		{
			delete device;
			return;
		}
		if (!device->initialize_partitions())
		{
			delete device;
			return;
		}
		MUST(m_devices.push_back(device));
	}


	DiskDevice::Partition::Partition(DiskDevice& device, const GUID& type, const GUID& guid, uint64_t start, uint64_t end, uint64_t attr, const char* name)
		: m_device(device)
		, m_type(type)
		, m_guid(guid)
		, m_lba_start(start)
		, m_lba_end(end)
		, m_attributes(attr)
	{
		memcpy(m_name, name, sizeof(m_name));
	}

	bool DiskDevice::Partition::read_sectors(uint32_t lba, uint32_t sector_count, uint8_t* buffer)
	{
		const uint32_t sectors_in_partition = m_lba_end - m_lba_start;
		ASSERT(lba + sector_count < sectors_in_partition);
		return m_device.read_sectors(m_lba_start + lba, sector_count, buffer);
	}

}