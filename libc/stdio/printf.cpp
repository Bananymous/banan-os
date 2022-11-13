#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t len)
{
	const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
	for(size_t i = 0; i < len; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static bool print_int(int value, size_t& out_len)
{
	if (value == -2147483648)
	{
		if (!print("-2147483648", 11))
			return false;
		out_len = 11;
		return true;
	}

	// Enough for any (32-bit) integer value
	char buffer[11];
	char* ptr = buffer + sizeof(buffer);
	int len = 0;
	bool sign = false;

	if (value < 0)
	{
		sign = true;
		value = -value;
	}

	while (value)
	{
		*(--ptr) = (value % 10) + '0';
		value /= 10;
		len++;
	}

	if (sign)
	{
		*(--ptr) = '-';
		len++;
	}

	if (!print(ptr, len))
		return false;

	out_len = len;
	return true;
}

static char bits_to_hex(unsigned char bits)
{
	if (bits < 10)
		return bits + '0';
	return bits - 10 + 'a';
}

static bool print_ptr(void* ptr, size_t& out_len)
{
	ptrdiff_t addr = reinterpret_cast<ptrdiff_t>(ptr);

	char buffer[2 + sizeof(ptrdiff_t) * 2];
	buffer[0] = '0';
	buffer[1] = 'x';

	size_t bytes = sizeof(ptrdiff_t);

	for (size_t i = 1; i <= bytes; i++)
	{
		unsigned char byte = (addr >> ((bytes - i) * 8)) & 0xff;
		buffer[i * 2 + 0] = bits_to_hex(byte >> 4);
		buffer[i * 2 + 1] = bits_to_hex(byte & 0b1111);
	}

	if (!print(buffer, 2 + bytes * 2))
		return false;

	out_len = 2 + bytes * 2;
	return true;
}

int printf(const char* __restrict format, ...)
{
	va_list args;
	va_start(args, format);

	int written = 0;

	while (*format)
	{
		size_t max_rem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%')
		{
			if (format[0] == '%')
				format++;
			size_t len = 1;
			while (format[len] && format[len] != '%')
				len++;
			if (!print(format, len))
				return -1;
			format += len;
			written += len;
			continue;
		}

		const char* format_start = format++;

		if (*format == 'c')
		{
			format++;
			char c = (char)va_arg(args, int);
			if (!max_rem) // FIXME: EOVERFLOW
				return -1;
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		}
		else if (*format == 's')
		{
			format++;
			const char* str = va_arg(args, const char*);
			size_t len = strlen(str);
			if (max_rem < len) // FIXME: EOVERFLOW
				return -1;
			if (!print(str, len))
				return -1;
			written += len;
		}
		else if (*format == 'd' || *format == 'i')
		{
			format++;
			int value = va_arg(args, int);
			size_t len;
			if (!print_int(value, len))
				return -1;
			written += len;
		}
		else if (*format == 'p')
		{
			format++;
			void* const ptr = va_arg(args, void*);
			size_t len;
			if (!print_ptr(ptr, len))
				return -1;
			written += len;
		}
		else
		{
			format = format_start;
			size_t len = strlen(format);
			if (max_rem < len) // FIXME: EOVERFLOW
				return -1;
			if (!print(format, len))
				return -1;
			written += len;
			format += len;
		}
	}

	va_end(args);
	return written;
}