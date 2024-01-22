#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Traits.h>

#include <ctype.h>
#include <scanf_impl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum class LengthModifier
{
	none,
	hh,
	h,
	l,
	ll,
	j,
	z,
	t,
	L,
};

struct Conversion
{
	bool			suppress	= false;
	int				field_width	= -1;
	bool			allocate	= false;
	LengthModifier	length		= LengthModifier::none;
	char			conversion	= '\0'; 
};

Conversion parse_conversion_specifier(const char*& format)
{
	Conversion conversion;

	if (*format == '*')
	{
		conversion.suppress = true;
		format++;
	}

	if (isdigit(*format))
	{
		conversion.field_width = 0;
		while (isdigit(*format))
		{
			conversion.field_width = (conversion.field_width * 10) + (*format - '0');
			format++;
		}
	}

	if (*format == 'm')
	{
		conversion.allocate = true;
		format++;
	}

	if (*format == 'h')
	{
		conversion.length = LengthModifier::h;
		format++;
		if (*format == 'h')
		{
			conversion.length = LengthModifier::hh;
			format++;
		}
	}
	else if (*format == 'l')
	{
		conversion.length = LengthModifier::l;
		format++;
		if (*format == 'l')
		{
			conversion.length = LengthModifier::ll;
			format++;
		}
	}
	else if (*format == 'j')
	{
		conversion.length = LengthModifier::j;
		format++;
	}
	else if (*format == 'z')
	{
		conversion.length = LengthModifier::z;
		format++;
	}
	else if (*format == 't')
	{
		conversion.length = LengthModifier::t;
		format++;
	}
	else if (*format == 'L')
	{
		conversion.length = LengthModifier::L;
		format++;
	}

	conversion.conversion = *format;
	format++;

	return conversion;
}

template<int BASE>
using BASE_TYPE = BAN::integral_constant<int, BASE>;

template<bool UNSIGNED>
using IS_UNSIGNED = BAN::integral_constant<bool, UNSIGNED>;

int scanf_impl(const char* format, va_list arguments, int (*__getc_fun)(void*), void* data)
{
	static constexpr int DONE = -1;
	static constexpr int NONE = -2;

	int nread = 0;
	int conversions = 0;
	int in = NONE;

	enum class ConversionResult
	{
		NONE,
		SUCCESS,
		INPUT_FAILURE,
		MATCH_FAILURE,
	};

	auto get_input =
		[&](bool advance) -> void
		{
			if (in == DONE)
				return;
			if (advance || in == NONE)
			{
				in = __getc_fun(data);
				nread++;
			}
		};

	auto parse_integer_internal =
		[&get_input, &in]<int BASE, typename T>(BASE_TYPE<BASE>, bool negative, int width, T* out) -> ConversionResult
		{
			constexpr auto is_base_digit =
				[](char c) -> bool
				{
					c = tolower(c);
					if ('0' <= c && c <= '9')
						return c - '0' < BASE;
					if ('a' <= c && c <= 'z')
						return c - 'a' + 10 < BASE;
					return false;
				};
			constexpr auto get_base_digit = [](char c) -> T { if (c <= '9') return c - '0'; return tolower(c) - 'a' + 10; };

			if (!is_base_digit(in))
				return ConversionResult::MATCH_FAILURE;

			*out = 0;
			while (width-- && is_base_digit(in))
			{
				*out = (*out * BASE) + get_base_digit(in);
				get_input(true);
			}

			if (negative)
				*out = -*out;

			return ConversionResult::SUCCESS;
		};

	auto parse_integer_typed =
		[&parse_integer_internal, &arguments, &get_input, &in]<int BASE, typename T>(BASE_TYPE<BASE>, bool suppress, int width, T*) -> ConversionResult
		{
			T dummy;
			T* out = suppress ? &dummy : va_arg(arguments, T*);

			bool negative = (in == '-');
			if (in == '-' || in == '+')
			{
				get_input(true);
				if (--width == 0)
					return ConversionResult::MATCH_FAILURE;
			}

			if constexpr(BASE == 0)
			{
				if (in != '0')
					return parse_integer_internal(BASE_TYPE<10>{}, negative, width, out);
				else
				{
					get_input(true);
					if (--width == 0)
					{
						*out = 0;
						return ConversionResult::SUCCESS;
					}
					if ('0' <= in && in <= '7')
						return parse_integer_internal(BASE_TYPE<8>{}, negative, width, out);
					if (tolower(in) == 'x')
					{
						get_input(true);
						if (--width == 0)
							return ConversionResult::MATCH_FAILURE;
						return parse_integer_internal(BASE_TYPE<16>{}, negative, width, out);
					}
					*out = 0;
					return ConversionResult::SUCCESS;
				}
			}

			if constexpr(BASE == 16)
			{
				if (in == '0')
				{
					get_input(true);
					width--;
					if (tolower(in) == 'x')
					{
						get_input(true);
						width--;
					}
					if (width <= 0)
					{
						*out = 0;
						return ConversionResult::SUCCESS;
					}
				}
			}

			return parse_integer_internal(BASE_TYPE<BASE>{}, negative, width, out);
		};

	auto parse_integer =
		[&parse_integer_typed, &get_input, &in]<int BASE, bool UNSIGNED>(BASE_TYPE<BASE>, IS_UNSIGNED<UNSIGNED>, bool suppress, int width, LengthModifier length) -> ConversionResult
		{
			get_input(false);
			while (isspace(in))
				get_input(true);
			if (in == DONE)
				return ConversionResult::INPUT_FAILURE;

			if (width == -1)
				width = __INT_MAX__;

			switch (length)
			{
				case LengthModifier::none:	return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, unsigned int*,		int*>			{});
				case LengthModifier::hh:	return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, unsigned char*,		signed char*>	{});
				case LengthModifier::h:		return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, unsigned short*,		short*>			{});
				case LengthModifier::l:		return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, unsigned long*,		long*>			{});
				case LengthModifier::ll:	return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, unsigned long long*,	long long*>		{});
				case LengthModifier::j:		return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, uintmax_t*,			intmax_t*>		{});
				case LengthModifier::z:		return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, size_t*,				ssize_t*>		{});
				case LengthModifier::t:		return parse_integer_typed(BASE_TYPE<BASE>{}, suppress, width, BAN::either_or_t<UNSIGNED, size_t*,				ptrdiff_t*>		{});
											static_assert(sizeof(size_t) == sizeof(ptrdiff_t));
				default:
					return ConversionResult::MATCH_FAILURE;
			}
		};

	auto parse_string =
		[&arguments, &get_input, &in](uint8_t* mask, bool exclude, bool suppress, bool allocate, int min_len, int max_len, bool terminate) -> ConversionResult
		{
			char* temp_dummy;
			char** outp = nullptr;
			if (suppress)
				;
			else if (allocate)
			{
				outp = va_arg(arguments, char**);
				*outp = (char*)malloc(BUFSIZ);
				if (*outp == nullptr)
					return ConversionResult::MATCH_FAILURE;
			}
			else
			{
				temp_dummy = va_arg(arguments, char*);
				outp = &temp_dummy;
			}

			const uint8_t xor_mask = exclude ? 0xFF : 0x00;

			get_input(false);
			if (in == DONE)
			{
				if (allocate)
					free(*outp);
				*outp = nullptr;
				return ConversionResult::INPUT_FAILURE;
			}

			int len = 0;
			while (len < max_len && in != DONE && ((mask[in / 8] ^ xor_mask) & (1 << (in % 8))))
			{
				len++;
				if (!suppress)
				{
					(*outp)[len - 1] = in;
					if (allocate && len % BUFSIZ == 0)
					{
						char* newp = (char*)realloc(*outp, len + BUFSIZ);
						if (newp == nullptr)
						{
							free(*outp);
							*outp = nullptr;
							return ConversionResult::MATCH_FAILURE;
						}
						*outp = newp;
					}
				}
				get_input(true);
			}
			if (len < min_len)
			{
				if (allocate)
					free(*outp);
				*outp = nullptr;
				return ConversionResult::MATCH_FAILURE;
			}

			if (!suppress && terminate)
				(*outp)[len] = '\0';

			return ConversionResult::SUCCESS;
		};

	while (isspace(*format) || isprint(*format))
	{
		if (*format == '%')
		{
			format++;
			auto conversion = parse_conversion_specifier(format);

			ConversionResult result = ConversionResult::NONE;
			switch (conversion.conversion)
			{
				case 'd': result = parse_integer(BASE_TYPE<10>{}, IS_UNSIGNED<false>{}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'i': result = parse_integer(BASE_TYPE<0> {}, IS_UNSIGNED<false>{}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'o': result = parse_integer(BASE_TYPE<8> {}, IS_UNSIGNED<true> {}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'u': result = parse_integer(BASE_TYPE<10>{}, IS_UNSIGNED<true> {}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'x': result = parse_integer(BASE_TYPE<16>{}, IS_UNSIGNED<true> {}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'X': result = parse_integer(BASE_TYPE<16>{}, IS_UNSIGNED<true> {}, conversion.suppress, conversion.field_width, conversion.length); break;
				case 'p': result = parse_integer(BASE_TYPE<16>{}, IS_UNSIGNED<true> {}, conversion.suppress, conversion.field_width, LengthModifier::j); break;
				case 'S':
					conversion.length = LengthModifier::l;
					// fall through
				case 's':
				{
					int width = conversion.field_width;
					if (width == -1)
						width = __INT_MAX__;
					uint8_t mask[0x100 / 8] {};
					for (int i = 0; i < 0x100; i++)
						if (isspace(i))
							mask[i / 8] |= 1 << (i % 8);
					get_input(false);
					while (isspace(in))
						get_input(true);
					result = parse_string(mask, true, conversion.suppress, conversion.allocate, 1, width, true);
					break;
				}
				case 'C':
					conversion.length = LengthModifier::l;
					// fall through
				case 'c':
				{
					int width = conversion.field_width;
					if (width == -1)
						width = 1;
					uint8_t mask[0x100 / 8] {};
					result = parse_string(mask, true, conversion.suppress, conversion.allocate, width, width, false);
					break;
				}
				case '[':
				{
					int width = conversion.field_width;
					if (width == -1)
						width = __INT_MAX__;

					bool exclude = (*format == '^');
					if (exclude)
						format++;

					uint8_t mask[0x100 / 8] {};
					if (*format == ']')
					{
						mask[']' / 8] |= 1 << (']' % 8);
						format++;
					}

					bool first = true;
					while (*format && *format != ']')
					{
						if (!first && *format == '-' && *(format + 1) != ']')
						{
							int min = BAN::Math::min(*(format - 1), *(format + 1));
							int max = BAN::Math::max(*(format - 1), *(format + 1));
							for (int i = min; i <= max; i++)
								mask[i / 8] |= 1 << (i % 8);
							format += 2;
						}
						else
						{
							mask[*format / 8] |= 1 << (*format % 8);
							format++;
						}
						first = false;
					}

					if (*format == ']')
						result = parse_string(mask, exclude, conversion.suppress, conversion.allocate, 1, width, true);
					else
						result = ConversionResult::MATCH_FAILURE;
					format++;
					break;
				}
				case 'n':
					if (!conversion.suppress)
						*va_arg(arguments, int*) = nread - (in != NONE);
					conversion.suppress = true; // Dont count this as conversion
					result = ConversionResult::SUCCESS;
					break;
				case '%':
					get_input(false);
					if (in == DONE)
					{
						result = ConversionResult::INPUT_FAILURE;
						break;
					}
					if (in != '%')
					{
						result = ConversionResult::MATCH_FAILURE;
						break;
					}
					get_input(true);
					result = ConversionResult::SUCCESS;
					break;
				default:
					result = ConversionResult::MATCH_FAILURE;
					break;
			}

			ASSERT(result != ConversionResult::NONE);

			if (result == ConversionResult::INPUT_FAILURE && conversions == 0)
				return EOF;
			if (result != ConversionResult::SUCCESS)
				return conversions;
			if (!conversion.suppress)
				conversions++;
		}
		else if (isspace(*format))
		{
			get_input(false);
			while (isspace(in))
				get_input(true);
			format++;
		}
		else
		{
			get_input(false);
			if (in != *format)
				break;
			get_input(true);
			format++;
		}
	}

	return conversions;
}
