#pragma once

#include <BAN/Errors.h>

namespace Kernel
{

	BAN::ErrorOr<void> read_from_user(const void* user_addr, void* out, size_t size);
	BAN::ErrorOr<void> read_string_from_user(const char* user_addr, char* out, size_t max_size);
	BAN::ErrorOr<uint32_t> read_atomic_u32_from_user(const uint32_t* user_addr);
	BAN::ErrorOr<void> write_to_user(void* user_addr, const void* in, size_t size);

};
