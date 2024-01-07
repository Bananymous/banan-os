#pragma once

#include <BAN/StringView.h>

namespace Kernel
{

	enum class ErrorCode : uint32_t
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
		ATA_NoLBA,
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

	BAN::StringView error_string(ErrorCode);

}