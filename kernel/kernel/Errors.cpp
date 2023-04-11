#include <kernel/Errors.h>

namespace Kernel
{

	static BAN::StringView s_error_strings[] {
		"No Error"sv,
		"ACPI could not find root SDT header"sv,
		"ACPI no such header"sv,
		"ACPI root invalid",
		"PS/2 device timeout"sv,
		"PS/2 controller self test failed"sv,
		"PS/2 reset failed"sv,
		"PS/2 unsupported device"sv,
		"Invalid ext2 filesystem"sv,
		"Ext2 filesystem corrupted"sv,
		"Ext2 filesystem out of inodes"sv,
		"Attempted to access outside of device boundaries"sv,
		"Device has invalid GPT header"sv,
		"Device does not support LBA addressing"sv,
		"Address mark not found"sv,
		"Track zero not found"sv,
		"Aborted command"sv,
		"Media change request"sv,
		"ID not found"sv,
		"Media changed"sv,
		"Uncorrectable data error"sv,
		"Bad Block detected"sv,
		"Font file too small"sv,
		"Unsupported font format"sv,
	};
	static_assert(sizeof(s_error_strings) / sizeof(*s_error_strings) == (size_t)ErrorCode::Count);

	BAN::StringView error_string(ErrorCode error)
	{
		ASSERT((uint32_t)error < (uint32_t)ErrorCode::Count);
		return s_error_strings[(uint32_t)error];
	}

}