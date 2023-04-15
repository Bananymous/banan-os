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

		struct FADT : public SDTHeader
		{
			uint32_t firmware_ctrl;
			uint32_t dsdt;
			uint8_t __reserved;
			uint8_t preferred_pm_profile;
			uint16_t sci_int;
			uint32_t smi_cmd;
			uint8_t acpi_enable;
			uint8_t acpi_disable;
			uint8_t s4bios_req;
			uint8_t pstate_cnt;
			uint32_t pm1a_evt_blk;
			uint32_t pm1b_evt_blk;
			uint32_t pm1a_cnt_blk;
			uint32_t pm1b_cnt_blk;
			uint32_t pm2_cnt_blk;
			uint32_t pm_tmr_blk;
			uint32_t gpe0_blk;
			uint32_t gpe1_blk;
			uint8_t pm1_evt_len;
			uint8_t pm1_cnt_len;
			uint8_t pm2_cnt_len;
			uint8_t pm_tmr_len;
			uint8_t gpe0_blk_len;
			uint8_t gpe1_blk_len;
			uint8_t gpe1_base;
			uint8_t cst_cnt;
			uint16_t p_lvl2_lat;
			uint16_t p_lvl3_lat;
			uint16_t flush_size;
			uint16_t flush_stride;
			uint8_t duty_offset;
			uint8_t duty_width;
			uint8_t day_alrm;
			uint8_t mon_alrm;
			uint8_t century;
			uint16_t iapc_boot_arch;
			uint8_t __reserved2;
			uint32_t flags;
			uint8_t reset_reg[12];
			uint8_t reset_value;
			uint16_t arm_boot_arch;
			uint8_t fadt_minor_version;
			uint64_t x_firmware_version;
			uint64_t x_dsdt;
			uint8_t x_pm1a_evt_blk[12];
			uint8_t x_pm1b_evt_blk[12];
			uint8_t x_pm1a_cnt_blk[12];
			uint8_t x_pm1b_cnt_blk[12];
			uint8_t x_pm2_cnt_blk[12];
			uint8_t x_pm_tmr_blk[12];
			uint8_t x_gpe0_blk[12];
			uint8_t x_gpe1_blk[12];
			uint8_t sleep_control_reg[12];
			uint8_t sleep_status_reg[12];
			uint64_t hypervison_vendor_identity;
		} __attribute__((packed));

	public:
		static BAN::ErrorOr<void> initialize();
		static ACPI& get();

		const SDTHeader* get_header(const char[4]);

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