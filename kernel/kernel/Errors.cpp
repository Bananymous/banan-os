#include <BAN/Assert.h>
#include <kernel/Errors.h>

#include <stdint.h>
#include <stddef.h>

namespace Kernel
{

	static const char* s_error_strings[] {
		"No Error",
		"ACPI could not find root SDT header",
		"ACPI no such header",
		"ACPI root invalid",
		"Invalid ext2 filesystem",
		"Ext2 filesystem corrupted",
		"Ext2 filesystem out of inodes",
		"Attempted to access outside of device boundaries",
		"Device has invalid GPT header",
		"Device does not support LBA addressing",
		"Address mark not found",
		"Track zero not found",
		"Aborted command",
		"Media change request",
		"ID not found",
		"Media changed",
		"Uncorrectable data error",
		"Bad Block detected",
		"Unsupported ata device",
		"Font file too small",
		"Unsupported font format",
	};
	static_assert(sizeof(s_error_strings) / sizeof(*s_error_strings) == (size_t)ErrorCode::Count);

	const char* error_string(ErrorCode error)
	{
		ASSERT((uint32_t)error < (uint32_t)ErrorCode::Count);
		return s_error_strings[(uint32_t)error];
	}

}
