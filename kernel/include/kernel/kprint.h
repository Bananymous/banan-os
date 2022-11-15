#pragma once

#include <kernel/tty.h>

#include <stdint.h>
#include <string.h>


template<typename T>
static void kprint_signed(T value)
{
	if (value == 0)
	{
		terminal_putchar('0');
		return;
	}

	char  buffer[32];
	char* ptr = buffer + sizeof(buffer);
	bool  sign = false;
	
	if (value < 0)
	{
		sign = true;
		*(--ptr) = ((value % 10 + 10) % 10) + '0';
		value /= 10;
	}

	while (value)
	{
		*(--ptr) = (value % 10) + '0';
		value /= 10;
	}
	if (sign)
		*(--ptr) = '-';

	terminal_write(ptr, sizeof(buffer) - (ptr - buffer));
}

template<typename T>
static void kprint_unsigned(T value)
{
	if (value == 0)
	{
		terminal_putchar('0');
		return;
	}

	char  buffer[32];
	char* ptr = buffer + sizeof(buffer);

	while (value)
	{
		*(--ptr) = (value % 10) + '0';
		value /= 10;
	}

	terminal_write(ptr, sizeof(buffer) - (ptr - buffer));
}


template<typename T>
static void kprint_val(T)
{
	terminal_writestring("<unknown type>");
}

static void kprint(const char* format)
{
	terminal_writestring(format);
}

template<typename Arg, typename... Args>
static void kprint(const char* format, Arg arg, Args... args)
{
	const char* next = strstr(format, "{}");
	if (next == NULL)
	{
		terminal_writestring(format);
		return;
	}
	terminal_write(format, next - format);
	kprint_val<Arg>(arg);
	kprint(next + 2, args...);
}

template<> void kprint_val(short     int value)				{ kprint_signed(value); }
template<> void kprint_val(          int value)				{ kprint_signed(value); }
template<> void kprint_val(long      int value)				{ kprint_signed(value); }
template<> void kprint_val(long long int value)				{ kprint_signed(value); }

template<> void kprint_val(unsigned short     int value)	{ kprint_unsigned(value); }
template<> void kprint_val(unsigned           int value)	{ kprint_unsigned(value); }
template<> void kprint_val(unsigned long      int value)	{ kprint_unsigned(value); }
template<> void kprint_val(unsigned long long int value)	{ kprint_unsigned(value); }

template<> void kprint_val(         char value)				{ terminal_putchar(value); }
template<> void kprint_val(signed   char value)				{ kprint_signed(value); }
template<> void kprint_val(unsigned char value)				{ kprint_unsigned(value); }

template<> void kprint_val(const char* value)				{ terminal_writestring(value); }
template<> void kprint_val(char*       value)				{ terminal_writestring(value); }



static char bits_to_hex(uint8_t val)
{
	val = val & 0xF;
	if (val < 10)
		return val + '0';
	return val + 'a' - 10;
}

template<> void kprint_val(void* value)
{
	terminal_write("0x", 2);

	if constexpr(sizeof(void*) == sizeof(uint32_t))
	{
		uint32_t addr = (uint32_t)value;
		terminal_putchar(bits_to_hex(addr >> 28));
		terminal_putchar(bits_to_hex(addr >> 24));
		terminal_putchar(bits_to_hex(addr >> 20));
		terminal_putchar(bits_to_hex(addr >> 16));
		terminal_putchar(bits_to_hex(addr >> 12));
		terminal_putchar(bits_to_hex(addr >>  8));
		terminal_putchar(bits_to_hex(addr >>  4));
		terminal_putchar(bits_to_hex(addr >>  0));
	}
	else
	{
		uint64_t addr = (uint64_t)value;
		terminal_putchar(bits_to_hex(addr >> 60));
		terminal_putchar(bits_to_hex(addr >> 56));
		terminal_putchar(bits_to_hex(addr >> 52));
		terminal_putchar(bits_to_hex(addr >> 48));
		terminal_putchar(bits_to_hex(addr >> 44));
		terminal_putchar(bits_to_hex(addr >> 40));
		terminal_putchar(bits_to_hex(addr >> 36));
		terminal_putchar(bits_to_hex(addr >> 32));
		terminal_putchar(bits_to_hex(addr >> 28));
		terminal_putchar(bits_to_hex(addr >> 24));
		terminal_putchar(bits_to_hex(addr >> 20));
		terminal_putchar(bits_to_hex(addr >> 16));
		terminal_putchar(bits_to_hex(addr >> 12));
		terminal_putchar(bits_to_hex(addr >>  8));
		terminal_putchar(bits_to_hex(addr >>  4));
		terminal_putchar(bits_to_hex(addr >>  0));
	}
}
