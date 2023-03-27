#pragma once

#include <BAN/Errors.h>

namespace Kernel
{

	class ACPI
	{
	public:
		struct SDTHeader
		{
			uint8_t signature[4];
			uint32_t length;
			uint8_t revision;
			uint8_t checksum;
			uint8_t oemid[6];
			uint64_t oem_table_id;
			uint32_t oem_revision;
			uint32_t creator_id;
			uint32_t creator_revision;
		} __attribute__((packed));

	public:
		static BAN::ErrorOr<void> initialize();
		static ACPI& get();

		BAN::ErrorOr<const SDTHeader*> get_header(const char[4]);
		void unmap_header(const SDTHeader*);

	private:
		ACPI() = default;
		BAN::ErrorOr<void> initialize_impl();

		const SDTHeader* get_header_from_index(size_t);

	private:
		uintptr_t m_header_table = 0;
		uint32_t m_entry_size = 0;
		uint32_t m_entry_count = 0;
	};

}