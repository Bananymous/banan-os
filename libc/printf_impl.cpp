#include <BAN/Traits.h>
#include <BAN/Math.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BAN_PRINTF_PUTC(c)						\
	do											\
	{											\
		if (putc_fun((c), data) == EOF) 		\
			return -1;							\
		written++;								\
	} while (false)

struct format_options_t
{
	bool alternate_form { false };
	bool zero_padded { false };
	bool left_justified { false };
	bool show_plus_sign_as_space { false };
	bool show_plus_sign { false };
	int width { -1 };
	int percision { -1 };
};

template<BAN::integral T>
static void integer_to_string(char* buffer, T value, int base, bool upper, const format_options_t options)
{
	int width = 1;
	bool zero_padded = options.zero_padded;
	if (options.percision != -1)
	{
		width = options.percision;
		zero_padded = false;
	}
	else if (options.width != -1)
		width = options.width;

	auto digit_char = [](int digit, bool upper)
	{
		if (digit < 10)
			return '0' + digit;
		return (upper ? 'A' : 'a') + (digit - 10);
	};

	bool sign = false;
	int offset = 0;
	if (value < 0)
	{
		sign = true;
		buffer[offset++] = digit_char(-(value % base), upper);
		value = -(value / base);
	}

	while (value)
	{
		buffer[offset++] = digit_char(value % base, upper);
		value /= base;
	}

	int prefix_length = 0;

	if (sign || options.show_plus_sign || options.show_plus_sign_as_space)
		prefix_length++;
	if (base == 8 && options.alternate_form)
		prefix_length++;
	if (base == 16 && options.alternate_form)
		prefix_length += 2;

	while (offset < width - prefix_length)
		buffer[offset++] = (zero_padded ? '0' : ' ');

	if (sign)
		buffer[offset++] = '-';
	else if (options.show_plus_sign)
		buffer[offset++] = '+';
	else if (options.show_plus_sign_as_space)
		buffer[offset++] = ' ';

	if (base == 8 && options.alternate_form)
		buffer[offset++] = '0';
	if (base == 16 && options.alternate_form)
	{
		buffer[offset++] = 'x';
		buffer[offset++] = '0';
	}

	for (int i = 0; i < offset / 2; i++)
	{
		char temp = buffer[i];
		buffer[i] = buffer[offset - i - 1];
		buffer[offset - i - 1] = temp;
	}

	buffer[offset++] = '\0';
}

template<BAN::floating_point T>
static void floating_point_to_string(char* buffer, T value, bool upper, const format_options_t options)
{
	int percision = 6;
	if (options.percision != -1)
		percision = options.percision;

	int offset = 0;

	// Add sign if needed
	if (value < (T)0.0)
	{
		buffer[offset++] = '-';
		value = -value;
	}
	else if (options.show_plus_sign)
		buffer[offset++] = '+';
	else if (options.show_plus_sign_as_space)
		buffer[offset++] = ' ';

	// Round last digit
	value += (T)0.5 * BAN::Math::pow<T>(10.0, -percision);

	// Add integer part of the decimal
	if (value < (T)1.0)
	{
		buffer[offset++] = '0';
		value *= (T)10.0;
	}
	else
	{
		int exponent = (int)BAN::Math::log10<T>(value);
		T magnitude = BAN::Math::pow<T>(10, exponent);

		value /= magnitude;

		for (int i = 0; i <= exponent; i++)
		{
			int digit = 0;
			for (; digit < 10; digit++)
				if (value < digit + 1)
					break;
			buffer[offset++] = '0' + digit;
			value -= (T)digit;
			value *= (T)10.0;
		}
	}

	// We are done if the decimal part is not written
	if (percision == 0 && !options.alternate_form)
	{
		buffer[offset++] = '\0';
		return;
	}
	buffer[offset++] = '.';
	
	// Add the 'percision' digits after decimal point
	for (int i = 0; i < percision; i++)
	{
		int digit = 0;
		for (; digit < 10; digit++)
			if (value < digit + 1)
				break;
		buffer[offset++] = '0' + digit;
		value -= (T)digit;
		value *= (T)10.0;
	}

	buffer[offset++] = '\0';
}

template<BAN::floating_point T>
static void floating_point_to_exponent_string(char* buffer, T value, bool upper, const format_options_t options)
{
	int percision = 6;
	if (options.percision != -1)
		percision = options.percision;
	
	int offset = 0;

	// Add sign if needed
	if (value < (T)0.0)
	{
		buffer[offset++] = '-';
		value = -value;
	}
	else if (options.show_plus_sign)
		buffer[offset++] = '+';
	else if (options.show_plus_sign_as_space)
		buffer[offset++] = ' ';

	// Calculate which number to put as exponent
	int exponent = 0;
	if (value != (T)0.0)
	{
		exponent = (int)floorl(BAN::Math::log10<T>(value));
		value /= BAN::Math::pow<T>(10.0, exponent);
	}

	// Add first numbers before 'e'
	floating_point_to_string<T>(buffer + offset, value, upper, options);
	offset = strlen(buffer);

	// Add the exponent part
	buffer[offset++] = (upper ? 'E' : 'e');
	format_options_t exponent_options;
	exponent_options.show_plus_sign = true;
	exponent_options.zero_padded = true;
	exponent_options.width = 3;
	integer_to_string<int>(buffer + offset, exponent, 10, upper, exponent_options);
}

extern "C" int printf_impl(const char* format, va_list arguments, int (*putc_fun)(int, void*), void* data)
{
	int written = 0;
	while (*format)
	{
		if (*format == '%')
		{
			format_options_t options;

			// PARSE FLAGS
			bool flags_done = false;
			while (!flags_done)
			{
				format++;
				switch (*format)
				{
					case '#': options.alternate_form = true; break;
					case '0': options.zero_padded = true; break;
					case '-': options.left_justified = true; break;
					case ' ': options.show_plus_sign_as_space = true; break;
					case '+': options.show_plus_sign = true; break;
				default:
					flags_done = true;
					break;
				}
			}
			if (options.zero_padded && options.left_justified)
				options.zero_padded = false;

			// PARSE FIELD WIDTH
			if (*format == '*')
			{
				options.width = va_arg(arguments, int);
				format++;
			}
			else if (isdigit(*format) && *format != '0')
			{
				int width = 0;
				while (isdigit(*format))
				{
					width *= 10;
					width += *format - '0';
					format++;
				}
				options.width = width;
			}

			// PARSE PERCISION
			if (*format == '.')
			{
				format++;
				int percision = 0;
				if (isdigit(*format))
				{
					while (isdigit(*format))
					{
						percision *= 10;
						percision += *format - '0';
						format++;
					}
				}
				else if (*format == '*')
				{
					percision = va_arg(arguments, int);
				}
				if (percision < 0)
					percision = -1;
				options.percision = percision;
			}
		
			// TODO: Lenght modifier

			static char conversion[1024];

			const char* string = nullptr;
			switch (*format)
			{
			case 'd':
			case 'i':
			{
				int value = va_arg(arguments, int);
				integer_to_string<int>(conversion, value, 10, false, options);
				string = conversion;
				format++;
				break;
			}
			case 'o':
			{
				unsigned int value = va_arg(arguments, unsigned int);
				integer_to_string<unsigned int>(conversion, value, 8, false, options);
				string = conversion;
				format++;
				break;
			}
			case 'u':
			{
				unsigned int value = va_arg(arguments, unsigned int);
				integer_to_string<unsigned int>(conversion, value, 10, false, options);
				string = conversion;
				format++;
				break;
			}
			case 'x':
			case 'X':
			{
				unsigned int value = va_arg(arguments, unsigned int);
				integer_to_string<unsigned int>(conversion, value, 16, *format == 'X', options);
				string = conversion;
				format++;
				break;
			}
			case 'e':
			case 'E':
			{
				double value = va_arg(arguments, double);
				floating_point_to_exponent_string<double>(conversion, value, *format == 'E', options);
				string = conversion;
				format++;
				break;
			}
			case 'f':
			case 'F':
			{
				double value = va_arg(arguments, double);
				floating_point_to_string<double>(conversion, value, *format == 'F', options);
				string = conversion;
				format++;
				break;
			}
			case 'g':
			case 'G':
				// TODO
				break;
			case 'a':
			case 'A':
				// TODO
				break;
			case 'c':
			{
				conversion[0] = va_arg(arguments, int);
				conversion[1] = '\0';
				string = conversion;
				format++;
				break;
			}
			case 's':
			{
				string = va_arg(arguments, const char*);
				format++;
				break;
			}
			case 'C':
				// TODO (depricated)
				break;
			case 'S':
				// TODO (depricated)
				break;
			case 'p':
			{
				void* ptr = va_arg(arguments, void*);
				options.alternate_form = true;
				integer_to_string<uintptr_t>(conversion, (uintptr_t)ptr, 16, false, options);
				string = conversion;
				format++;
				break;
			}
			case 'n':
			{
				int* target = va_arg(arguments, int*);
				*target = written;
				format++;
				break;
			}
			case 'm':
			{
				// NOTE: this is a glibc extension
				if (options.alternate_form)
					string = strerrorname_np(errno);
				else
					string = strerror(errno);
				format++;
				break;
			}
			case '%':
			{
				conversion[0] = '%';
				conversion[1] = '\0';
				string = conversion;
				format++;
				break;
			}
			default:
				break;
			}

			if (string)
			{
				int len = strlen(string);

				if (options.width == -1)
					options.width = 0;

				if (!options.left_justified)
					for (int i = len; i < options.width; i++)
						BAN_PRINTF_PUTC(' ');

				while (*string)
				{
					BAN_PRINTF_PUTC(*string);
					string++;
				}

				if (options.left_justified)
					for (int i = len; i < options.width; i++)
						BAN_PRINTF_PUTC(' ');
			}
		}
		else
		{
			BAN_PRINTF_PUTC(*format);
			format++;
		}
	}
	
	return written;
}
