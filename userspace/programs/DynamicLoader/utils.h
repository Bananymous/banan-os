#pragma once

#include <BAN/Traits.h>

#include <unistd.h> // make sure syscall function is declared before defining it

#include <errno.h>
#include <kernel/API/Syscall.h>

#define syscall(...) ({ \
		long _ret = -ERESTART; \
		while (_ret == -ERESTART) \
			_ret = _kas_syscall(__VA_ARGS__); \
		_ret; \
	})

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

size_t strlen(const char* s);

int strcmp(const char* s1, const char* s2);
char* strcpy(char* __restrict s1, const char* __restrict s2);

void* memcpy(void* __restrict s1, const void* __restrict s2, size_t n);
void* memset(void* s, int c, size_t n);

void init_random();
void fini_random();
uintptr_t get_random_uptr();
