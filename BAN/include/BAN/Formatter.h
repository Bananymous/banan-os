#pragma once

#include <BAN/Move.h>

#include <stdint.h>
#include <stddef.h>

namespace BAN::Formatter
{

	struct ValueFormat;

	template<typename F>
	static void print(F putc, const char* format);

	template<typename F, typename Arg, typename... Args>
	static void print(F putc, const char* format, Arg&& arg, Args&&... args);

	template<typename F, typename... Args>
	static void println(F putc, const char* format, Args&&... args);

	template<typename F, typename T>
	static void print_argument(F putc, T value, const ValueFormat& format);

	namespace detail
	{
		template<typename F, typename T>
		static size_t parse_format_and_print_argument(F putc, const char* format, T&& arg);
	}


	/*
	
		IMPLEMENTATION

	*/

	struct ValueFormat
	{
		int base		= 10;
		int percision	= 3;
		int fill		= 0;
		bool upper		= false;
	};
	
	template<typename F>
	void print(F putc, const char* format)
	{
		while (*format)
		{
			putc(*format);
			format++;
		}
	}

	template<typename F, typename Arg, typename... Args>
	void print(F putc, const char* format, Arg&& arg, Args&&... args)
	{
		while (*format && *format != '{')
		{
			putc(*format);
			format++;
		}

		if (*format == '{')
		{
			size_t arg_len = detail::parse_format_and_print_argument(putc, format, forward<Arg>(arg));
			if (arg_len == size_t(-1))
				return print(putc, format);
			print(putc, format + arg_len, forward<Args>(args)...);
		}
	}

	template<typename F, typename... Args>
	void println(F putc, const char* format, Args&&... args)
	{
		print(putc, format, args...);
		putc('\n');
	}

	namespace detail
	{

		template<typename F, typename Arg>
		size_t parse_format_and_print_argument(F putc, const char* format, Arg&& argument)
		{
			ValueFormat value_format;

			if (format[0] != '{')
				return size_t(-1);

			size_t i = 1;
			do
			{
				if (!format[i] || format[i] == '}')
					break;

				if ('0' <= format[i] && format[i] <= '9')
				{
					int fill = 0;
					while ('0' <= format[i] && format[i] <= '9')
					{
						fill = (fill * 10) + (format[i] - '0');
						i++;
					}
					value_format.fill = fill;
				}

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

			print_argument(putc, forward<Arg>(argument), value_format);

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

		template<typename F, typename T>
		void print_integer(F putc, T value, const ValueFormat& format)
		{
			if (value == 0)
			{
				for (int i = 0; i < format.fill || i < 1; i++)
					putc('0');
				return;
			}

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

			while (ptr >= buffer + sizeof(buffer) - format.fill)
				*(--ptr) = '0';

			if (sign)
				*(--ptr) = '-';
			
			print(putc, ptr);
		}

		template<typename F, typename T>
		void print_floating(F putc, T value, const ValueFormat& format)
		{
			int64_t int_part = (int64_t)value;
			T frac_part = value - (T)int_part;
			if (frac_part < 0)
				frac_part = -frac_part;

			print_integer(putc, int_part, format);
			
			if (format.percision > 0)
				putc('.');
			
			for (int i = 0; i < format.percision; i++)
			{
				frac_part *= format.base;
				if (i == format.percision - 1)
					frac_part += 0.5;

				putc(value_to_base_char((uint8_t)frac_part % format.base, format.base, format.upper));
			}
		}

		template<typename F>
		void print_pointer(F putc, void* ptr, const ValueFormat& format)
		{
			uintptr_t value = (uintptr_t)ptr;
			print(putc, "0x");
			for (int i = sizeof(void*) * 8 - 4; i >= 0; i -= 4)
				putc(value_to_base_char((value >> i) & 0xF, 16, format.upper));
		}

	}

	/*
	
		TEMPLATE SPECIALIZATIONS

	*/

	template<typename F, integral		T> void print_argument(F putc, T value, const ValueFormat& format) { detail::print_integer(putc, value, format); }
	template<typename F, floating_point	T> void print_argument(F putc, T value, const ValueFormat& format) { detail::print_floating(putc, value, format); }
	template<typename F, pointer		T> void print_argument(F putc, T value, const ValueFormat& format) { detail::print_pointer(putc, (void*)value, format); }

	template<typename F> void print_argument(F putc, char			value, const ValueFormat&)	{ putc(value); }
	template<typename F> void print_argument(F putc, bool			value, const ValueFormat&)	{ print(putc, value ? "true" : "false"); }
	template<typename F> void print_argument(F putc, const char*	value, const ValueFormat&)	{ print(putc, value);}
	
	//template<typename F> void print_argument(F putc, signed char			value, const ValueFormat& format)	{ detail::print_integer(putc, value, format); }
	//template<typename F> void print_argument(F putc, unsigned char			value, const ValueFormat& format)	{ detail::print_integer(putc, value, format); }
	//template<typename F, typename T> void print_argument(F putc, T*			value, const ValueFormat& format)	{ detail::print_pointer(putc, (void*)value, format); }

}
