#include <BAN/Assert.h>

#if __is_kernel

#include <kernel/Panic.h>

[[noreturn]] void __ban_assertion_failed(const char* location, const char* msg)
{
	Kernel::panic_impl(location, msg);
}

#else

#include <BAN/Debug.h>

[[noreturn]] void __ban_assertion_failed(const char* location, const char* msg)
{
	derrorln("{}: {}", location, msg);
	__builtin_trap();
}

#endif
