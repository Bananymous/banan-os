#include <kernel/Memory/Types.h>
#include <kernel/UserCopy.h>

extern "C" bool safe_user_memcpy(void*, const void*, size_t);
extern "C" bool safe_user_strncpy(void*, const void*, size_t);
extern "C" bool safe_user_atomic_read32(uint32_t*, const uint32_t*);

namespace Kernel
{

static inline bool is_valid_user_address(const void* user_addr, size_t size)
{
	const vaddr_t user_vaddr = reinterpret_cast<vaddr_t>(user_addr);
	if (BAN::Math::will_addition_overflow<vaddr_t>(user_vaddr, size))
		return false;
	if (user_vaddr + size > USERSPACE_END)
		return false;
	return true;
}

BAN::ErrorOr<void> read_from_user(const void* user_addr, void* out, size_t size)
{
	if (!is_valid_user_address(user_addr, size))
		return BAN::Error::from_errno(EFAULT);
	if (!safe_user_memcpy(out, user_addr, size))
		return BAN::Error::from_errno(EFAULT);
	return {};
}

BAN::ErrorOr<void> read_string_from_user(const char* user_addr, char* out, size_t max_size)
{
	max_size = BAN::Math::min<size_t>(max_size, USERSPACE_END - reinterpret_cast<vaddr_t>(user_addr));
	if (!is_valid_user_address(user_addr, max_size))
		return BAN::Error::from_errno(EFAULT);
	if (!safe_user_strncpy(out, user_addr, max_size))
		return BAN::Error::from_errno(EFAULT);
	return {};
}

BAN::ErrorOr<uint32_t> read_atomic_u32_from_user(const uint32_t* user_addr)
{
	if (reinterpret_cast<vaddr_t>(user_addr) % alignof(uint32_t))
		return BAN::Error::from_errno(EINVAL);
	if (!is_valid_user_address(user_addr, sizeof(uint32_t)))
		return BAN::Error::from_errno(EFAULT);
	uint32_t value;
	if (!safe_user_atomic_read32(&value, user_addr))
		return BAN::Error::from_errno(EFAULT);
	return value;
}

BAN::ErrorOr<void> write_to_user(void* user_addr, const void* in, size_t size)
{
	if (!is_valid_user_address(user_addr, size))
		return BAN::Error::from_errno(EFAULT);
	if (!safe_user_memcpy(user_addr, in, size))
		return BAN::Error::from_errno(EFAULT);
	return {};
}

}
