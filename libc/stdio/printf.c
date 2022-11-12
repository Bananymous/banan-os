#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t len)
{
	const unsigned char* bytes = (const unsigned char*)data;
	for(size_t i = 0; i < len; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static bool print_int(int value, int* out_len)
{
	if (value == -2147483648)
	{
		if (!print("-2147483648", 11))
			return false;
		*out_len = 11;
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

	*out_len = len;
	return true;
}

int printf(const char* restrict format, ...)
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
			int len;
			if (!print_int(value, &len))
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