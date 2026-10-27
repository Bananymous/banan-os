#ifndef _BITS_STRFTIME_HPP
#define _BITS_STRFTIME_HPP 1

#include <BAN/Debug.h>

#include <bits/printf.h>
#include <langinfo.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

template<typename>
struct strftime_traits;

template<>
struct strftime_traits<char>
{
	static constexpr int    (&isdigit)(int)               = ::isdigit;
	static constexpr size_t (&strlen)(const char*)        = ::strlen;
	static constexpr char*  (&strcpy)(char*, const char*) = ::strcpy;
	static constexpr int    (&snprintf)(char*, size_t, const char*, ...) = ::snprintf;
};

template<>
struct strftime_traits<wchar_t>
{
	static constexpr int      (&isdigit)(wint_t)                  = ::iswdigit;
	static constexpr size_t   (&strlen)(const wchar_t*)           = ::wcslen;
	static constexpr wchar_t* (&strcpy)(wchar_t*, const wchar_t*) = ::wcscpy;

	static int snprintf(wchar_t* buffer, size_t max_size, const char* format, ...)
	{
		va_list arguments;
		va_start(arguments, format);

		struct print_info
		{
			wchar_t* buffer;
			size_t remaining;
		};
		print_info info { buffer, max_size };

		const int result = printf_impl(format, arguments,
			[](int c, void* _info) {
				print_info* info = static_cast<print_info*>(_info);
				if (info->remaining == 1)
				{
					*info->buffer = '\0';
					info->remaining = 0;
				}
				else if (info->remaining)
				{
					*info->buffer = c;
					info->buffer++;
					info->remaining--;
				}
				return 0;
			}, &info
		);

		*info.buffer = '\0';

		va_end(arguments);
		return result;
	}
};

template<typename CHAR1, typename CHAR2>
static size_t _strftime(CHAR1* __restrict s, size_t maxsize, const CHAR2* __restrict format, const struct tm* __restrict timeptr)
{
	using traits = strftime_traits<CHAR1>;

	size_t len = 0;

	struct conversion_t
	{
		CHAR2 flag = CHAR2('\0');
		int width = -1;
	};

	const auto is_leap_year = [](uint64_t year) {
		if (year % 400 == 0)
			return true;
		if (year % 100 == 0)
			return false;
		if (year % 4 == 0)
			return true;
		return false;
	};

	const auto append_string = [&s, &len, &maxsize](const char* string) -> bool {
		const size_t string_len = strlen(string);
		if (len + string_len >= maxsize)
			return false;
		for (size_t i = 0; i < string_len; i++)
			s[len + i] = CHAR1(string[i]);
		s[len += string_len] = CHAR1('\0');
		return true;
	};

	const auto append_month = [&append_string](int index, bool abbreviated) {
		const nl_item base = abbreviated ? ABMON_1 : MON_1;
		const char* string = "INVALID";
		if (index >= 0 && index < 12)
			string = nl_langinfo(base + index);
		return append_string(string);
	};

	const auto append_weekday = [&append_string](int index, bool abbreviated) {
		const nl_item base = abbreviated ? ABDAY_1 : DAY_1;
		const char* string = "INVALID";
		if (index >= 0 && index < 7)
			string = nl_langinfo(base + index);
		return append_string(string);
	};

	const auto append_value = [&s, &len, &maxsize]<typename T>(const char* format, T value) -> bool {
		const int written = traits::snprintf(s + len, maxsize - len, format, value);
		if (len + written >= maxsize)
			return false;
		len += written;
		return true;
	};

	const auto append_value_weird = [&append_string, &append_value](long long value, int flag, int width) -> bool {
		char format[32];
		char* ptr = format;
		*ptr++ = '%';
		if (flag == '+')
			*ptr++ = '+';
		*ptr++ = '0';
		if (width != -1)
			ptr += sprintf(ptr, "%d", width);
		*ptr++ = 'l';
		*ptr++ = 'l';
		*ptr++ = 'd';
		*ptr++ = '\0';

		// idk why but musl libc test says that +4Y -> 2016 and +10F -> 2016-01-03
		// i have no idea why the + is not printed in those cases :)
		if (width < 11 && flag == '+')
		{
			char temp_buffer[12];
			const int nprint = sprintf(temp_buffer, format, value);
			return append_string(temp_buffer + (nprint == width + 1));
		}

		return append_value(format, value);
	};

	while (*format && len < maxsize)
	{
		if (*format != CHAR2('%'))
		{
			s[len++] = CHAR1(*format++);
			continue;
		}

		format++;

		conversion_t conversion;
		if (*format == CHAR2('0') || *format == CHAR2('+'))
			conversion.flag = *format++;
		if (traits::isdigit(*format))
		{
			conversion.width = 0;
			while (traits::isdigit(*format))
			{
				conversion.width = (conversion.width * 10) + (*format - CHAR2('0'));
				format++;
			}
		}
		switch (*format)
		{
			case CHAR2('E'):
			case CHAR2('O'):
				dwarnln("TODO: strftime modifiers");
				format++;
				break;
		}

		switch (*format)
		{
			case CHAR2('a'):
				if (!append_weekday(timeptr->tm_wday, true))
					return 0;
				break;
			case CHAR2('A'):
				if (!append_weekday(timeptr->tm_wday, false))
					return 0;
				break;
			case CHAR2('b'):
			case CHAR2('h'):
				if (!append_month(timeptr->tm_mon, true))
					return 0;
				break;
			case CHAR2('B'):
				if (!append_month(timeptr->tm_mon, false))
					return 0;
				break;
			case CHAR2('c'):
				if (const size_t ret = _strftime(s + len, maxsize - len, nl_langinfo(D_T_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('C'):
			{
				if (conversion.width == -1)
					conversion.width = 2;

				char new_format[32];
				if (conversion.flag == CHAR2('+'))
					sprintf(new_format, "%%+0%dd", conversion.width);
				else
					sprintf(new_format, "%%0%dd", conversion.width);
				if (!append_value(new_format, (1900 + timeptr->tm_year) / 100))
					return 0;
				break;
			}
			case CHAR2('d'):
				if (!append_value("%02d", timeptr->tm_mday))
					return 0;
				break;
			case CHAR2('D'):
				if (const size_t ret = _strftime(s + len, maxsize - len, "%m/%d/%y", timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('e'):
				if (!append_value("%2d", timeptr->tm_mday))
					return 0;
				break;
			case CHAR2('F'):
			{
				// remove trailing "-mm-dd" from width
				if (conversion.width >= 6)
					conversion.width -= 6;

				char new_format[32];
				char* ptr = new_format;

				*ptr++ = '%';
				if (conversion.flag)
					*ptr++ = conversion.flag;
				if (conversion.width != -1)
					ptr += sprintf(ptr, "%d", conversion.width);
				strcpy(ptr, "Y-%m-%d");

				if (const size_t ret = _strftime(s + len, maxsize - len, new_format, timeptr))
					len += ret;
				else return 0;
				break;
			}
			case CHAR2('H'):
				if (!append_value("%02d", timeptr->tm_hour))
					return 0;
				break;
			case CHAR2('I'):
				if (!append_value("%02d", ((timeptr->tm_hour + 11) % 12) + 1))
					return 0;
				break;
			case CHAR2('j'):
				if (!append_value("%03d", timeptr->tm_yday + 1))
					return 0;
				break;
			case CHAR2('m'):
				if (!append_value("%02d", timeptr->tm_mon + 1))
					return 0;
				break;
			case CHAR2('M'):
				if (!append_value("%02d", timeptr->tm_min))
					return 0;
				break;
			case CHAR2('n'):
				s[len++] = CHAR1('\n');
				break;
			case CHAR2('p'):
				if (!append_string(timeptr->tm_hour < 12 ? nl_langinfo(AM_STR) : nl_langinfo(PM_STR)))
					return 0;
				break;
			case CHAR2('r'):
				if (const size_t ret = _strftime(s + len, maxsize - len, nl_langinfo(T_FMT_AMPM), timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('R'):
				if (const size_t ret = _strftime(s + len, maxsize - len, "%H:%M", timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('s'):
				if (struct tm tm_copy = *timeptr; !append_value("%llu", mktime(&tm_copy)))
					return 0;
				break;
			case CHAR2('S'):
				if (!append_value("%02d", timeptr->tm_sec))
					return 0;
				break;
			case CHAR2('t'):
				s[len++] = CHAR1('\t');
				break;
			case CHAR2('T'):
				if (const size_t ret = _strftime(s + len, maxsize - len, "%H:%M:%S", timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('u'):
				if (!append_value("%d", ((timeptr->tm_wday + 6) % 7) + 1))
					return 0;
				break;
			case CHAR2('U'):
				if (!append_value("%02d", (timeptr->tm_yday - timeptr->tm_wday + 7) / 7))
					return 0;
				break;
			case CHAR2('g'):
			case CHAR2('G'):
			case CHAR2('V'):
			{
				// Adapted from GNU libc implementation

				constexpr auto iso_week_days = [](int yday, int wday) -> int {
					return yday - (yday - wday + 382) % 7 + 3;
				};

				int year = timeptr->tm_year + 1900;
				int days = iso_week_days(timeptr->tm_yday, timeptr->tm_wday);

				if (days < 0)
				{
					year--;
					days = iso_week_days(timeptr->tm_yday + (365 + is_leap_year(year)), timeptr->tm_wday);
				}
				else
				{
					int d = iso_week_days(timeptr->tm_yday - (365 + is_leap_year(year)), timeptr->tm_wday);
					if (d >= 0)
					{
						year++;
						days = d;
					}
				}

				switch (*format)
				{
					case CHAR2('g'):
						if (!append_value("%02d", ((year % 100) + 100) % 100))
							return 0;
						break;
					case CHAR2('G'):
						if (conversion.flag == CHAR2('\0') && 1900 + year > 9999)
							conversion.flag = CHAR2('+');
						if (conversion.width == -1)
							conversion.width = 4;
						if (!append_value_weird(year, conversion.flag, conversion.width))
							return 0;
						break;
					case CHAR2('V'):
						if (!append_value("%02d", days / 7 + 1))
							return 0;
						break;
				}
				break;
			}
			case CHAR2('w'):
				if (!append_value("%d", timeptr->tm_wday))
					return 0;
				break;
			case CHAR2('W'):
				if (!append_value("%02d", (timeptr->tm_yday - (timeptr->tm_wday - 1 + 7) % 7 + 7) / 7))
					return 0;
				break;
			case CHAR2('x'):
				if (const size_t ret = _strftime(s + len, maxsize - len, nl_langinfo(D_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('X'):
				if (const size_t ret = _strftime(s + len, maxsize - len, nl_langinfo(T_FMT), timeptr))
					len += ret;
				else return 0;
				break;
			case CHAR2('y'):
				if (!append_value("%02d", timeptr->tm_year % 100))
					return 0;
				break;
			case CHAR2('Y'):
				if (conversion.flag == CHAR2('\0') && timeptr->tm_year > 9999 - 1900)
					conversion.flag = CHAR2('+');
				if (conversion.width == -1)
					conversion.width = 4;
				if (!append_value_weird(1900ll + timeptr->tm_year, conversion.flag, conversion.width))
					return 0;
				break;
			case CHAR2('z'):
				// FIXME: support timezones
				break;
			case CHAR2('Z'):
				// FIXME: support timezones
				break;
			case CHAR2('%'):
				s[len++] = CHAR2('%');
				break;
		}

		format++;
	}

	if (*format != CHAR2('\0'))
		return 0;
	s[len] = CHAR1('\0');
	return len;
}

#endif
