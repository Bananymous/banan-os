#pragma once

namespace Kernel
{

	enum class ErrorCode
	{
		None,
		ACPI_NoRootSDT,
		ACPI_NoSuchHeader,
		ACPI_RootInvalid,
		Ext2_Invalid,
		Ext2_Corrupted,
		Ext2_NoInodes,
		Storage_Boundaries,
		Storage_GPTHeader,
		ATA_AMNF,
		ATA_TKZNF,
		ATA_ABRT,
		ATA_MCR,
		ATA_IDNF,
		ATA_MC,
		ATA_UNC,
		ATA_BBK,
		ATA_UnsupportedDevice,
		Font_FileTooSmall,
		Font_Unsupported,
		Count
	};

	const char* error_string(ErrorCode);

}
