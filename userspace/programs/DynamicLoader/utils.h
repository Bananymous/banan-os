#pragma once

#include <BAN/Traits.h>
#include <kernel/Syscall.h>

#include <stddef.h>
#include <stdint.h>

template<typename T>
inline constexpr T min(T a, T b)
{
	return a < b ? a : b;
}

template<typename T>
inline constexpr T max(T a, T b)
{
	return a > b ? a : b;
}

template<typename... Ts> requires (sizeof...(Ts) <= 5) && ((BAN::is_integral_v<Ts> || BAN::is_pointer_v<Ts>) && ...)
inline auto syscall(long syscall, Ts... args)
{
	return Kernel::syscall(syscall, (uintptr_t)args...);
}

void print(int fd, const char* buffer);

[[noreturn]] void print_error_and_exit(const char* message, int error);

template<BAN::unsigned_integral T>
inline void print_uint(int fd, T val, uint8_t base = 10)
{
	constexpr auto get_base_char = [](T val) { return ((val < 10) ? '0' : 'A' - 10) + val; };

	char buffer[32];
	char* ptr = buffer + sizeof(buffer);
	*--ptr = '\0';
	do { *--ptr = get_base_char(val % base); val /= base; } while (val);
	print(fd, ptr);
}

int strcmp(const char* s1, const char* s2);
char* strcpy(char* __restrict s1, const char* __restrict s2);

void* memcpy(void* __restrict s1, const void* __restrict s2, size_t n);
void* memset(void* s, int c, size_t n);

void init_random();
void fini_random();
uintptr_t get_random_uptr();
