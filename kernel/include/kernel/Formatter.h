#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Formatter
{

	struct ValueFormat;

	template<void(*PUTC_LIKE)(char)>
	static void print(const char* format);

	template<void(*PUTC_LIKE)(char), typename Arg, typename... Args>
	static void print(const char* format, Arg arg, Args... args);

	template<void(*PUTC_LIKE)(char), typename... Args>
	static void println(const char* format = "", Args... args);

	template<void(*PUTC_LIKE)(char), typename T>
	static size_t print_argument(const char* format, T arg);

	template<void(*PUTC_LIKE)(char), typename T>
	static void print_value(T value, const ValueFormat& format);


	/*
	
		IMPLEMENTATION

	*/

	struct ValueFormat
	{
		int base		= 10;
		int percision	= 3;
		bool upper		= false;
	};
	
	template<void(*PUTC_LIKE)(char)>
	void print(const char* format)
	{
		while (*format)
		{
			PUTC_LIKE(*format);
			format++;	
		}
	}

	template<void(*PUTC_LIKE)(char), typename Arg, typename... Args>
	void print(const char* format, Arg arg, Args... args)
	{
		while (*format && *format != '{')
		{
			PUTC_LIKE(*format);
			format++;
		}

		if (*format == '{')
		{
			size_t arg_len = print_argument<PUTC_LIKE>(format, arg);
			if (arg_len == size_t(-1))
				return print<PUTC_LIKE>(format);
			print<PUTC_LIKE>(format + arg_len, args...);
		}
	}

	template<void(*PUTC_LIKE)(char), typename... Args>
	void println(const char* format, Args... args)
	{
		print<PUTC_LIKE>(format, args...);
		PUTC_LIKE('\n');
	}


	template<void(*PUTC_LIKE)(char), typename Arg>
	size_t print_argument(const char* format, Arg argument)
	{
		ValueFormat value_format;

		if (format[0] != '{')
			return size_t(-1);

		size_t i = 1;
		do
		{
			if (!format[i] || format[i] == '}')
				break;

			switch (format[i])
			{
				case 'b': value_format.base = 2;	value_format.upper = false;	i++; break;
				case 'B': value_format.base = 2;	value_format.upper = true;	i++; break;
				case 'o': value_format.base = 8;	value_format.upper = false;	i++; break;
				case 'O': value_format.base = 8;	value_format.upper = true;	i++; break;
				case 'd': value_format.base = 10;	value_format.upper = false;	i++; break;
				case 'D': value_format.base = 10;	value_format.upper = true;	i++; break;
				case 'h': value_format.base = 16;	value_format.upper = false;	i++; break;
				case 'H': value_format.base = 16;	value_format.upper = true;	i++; break;
				default: break;
			}

			if (!format[i] || format[i] == '}')
				break;

			if (format[i] == '.')
			{
				i++;
				int percision = 0;
				while ('0' <= format[i] && format[i] <= '9')
				{
					percision = (percision * 10) + (format[i] - '0');
					i++;
				}
				value_format.percision = percision;
			}

		} while(false);

		if (format[i] != '}')
			return size_t(-1);

		print_value<PUTC_LIKE>(argument, value_format);

		return i + 1;
	}

	static char value_to_base_char(uint8_t value, int base, bool upper)
	{
		if (base <= 10)
			return value + '0';
		if (base <= 36)
		{
			if (value < 10)
				return value + '0';
			return value + (upper ? 'A' : 'a') - 10;
		}
		return '?';
	}

	template<void(*PUTC_LIKE)(char), typename T>
	void print_integer(T value, const ValueFormat& format)
	{
		if (value == 0)
			return PUTC_LIKE('0');

		bool sign = false;

		// Fits signed 64-bit binary number and null
		char  buffer[66];
		char* ptr = buffer + sizeof(buffer);
		*(--ptr) = '\0';

		if (value < 0)
		{
			sign = true;
			T digit = (format.base - (value % format.base)) % format.base;
			*(--ptr) = value_to_base_char(digit, format.base, format.upper);
			value = -(value / format.base);
		}

		while (value)
		{
			*(--ptr) = value_to_base_char(value % format.base, format.base, format.upper);
			value /= format.base;
		}

		if (sign)
			*(--ptr) = '-';
		
		print<PUTC_LIKE>(ptr);
	}

	template<void(*PUTC_LIKE)(char), typename T>
	void print_floating(T value, const ValueFormat& format)
	{
		int64_t int_part = (int64_t)value;
		T frac_part = value - (T)int_part;
		if (frac_part < 0)
			frac_part = -frac_part;

		print_integer<PUTC_LIKE>(int_part, format);
		
		if (format.percision > 0)
			PUTC_LIKE('.');
		
		for (int i = 0; i < format.percision; i++)
		{
			frac_part *= format.base;
			if (i == format.percision - 1)
				frac_part += 0.5;

			PUTC_LIKE(value_to_base_char((uint8_t)frac_part % format.base, format.base, format.upper));
		}
	}

	template<void(*PUTC_LIKE)(char)>
	void print_pointer(void* ptr, const ValueFormat& format)
	{
		uintptr_t value = (uintptr_t)ptr;
		print<PUTC_LIKE>("0x");
		for (int i = sizeof(void*) * 8 - 4; i >= 0; i -= 4)
			PUTC_LIKE(value_to_base_char((value >> i) & 0xF, 16, format.upper));
	}

	/*
	
		TEMPLATE SPECIALIZATIONS

	*/

	template<void(*PUTC_LIKE)(char)> void print_value(short					value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(int					value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(long					value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(long long				value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }

	template<void(*PUTC_LIKE)(char)> void print_value(unsigned short		value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(unsigned int			value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(unsigned long			value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(unsigned long long	value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }

	template<void(*PUTC_LIKE)(char)> void print_value(float					value, const ValueFormat& format)	{ print_floating<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(double				value, const ValueFormat& format)	{ print_floating<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(long double			value, const ValueFormat& format)	{ print_floating<PUTC_LIKE>(value, format); }
	
	template<void(*PUTC_LIKE)(char)> void print_value(char					value, const ValueFormat&)			{ PUTC_LIKE(value); }
	template<void(*PUTC_LIKE)(char)> void print_value(signed char			value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(unsigned char			value, const ValueFormat& format)	{ print_integer<PUTC_LIKE>(value, format); }

	template<void(*PUTC_LIKE)(char)> void print_value(bool					value, const ValueFormat& format)	{ print<PUTC_LIKE>(value ? "true" : "false"); }

	template<void(*PUTC_LIKE)(char), typename T> void print_value(T*		value, const ValueFormat& format)	{ print_pointer<PUTC_LIKE>((void*)value, format); }
	template<void(*PUTC_LIKE)(char)> void print_value(const char*			value, const ValueFormat&)			{ print<PUTC_LIKE>(value);}

}
