#include <BAN/Assert.h>

#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static locale_t s_current_locales[LC_ALL] {
	LOCALE_POSIX,
	LOCALE_POSIX,
	LOCALE_POSIX,
	LOCALE_POSIX,
	LOCALE_POSIX,
	LOCALE_POSIX,
};
static_assert(LC_ALL == 6);

static locale_t str_to_locale(const char* locale)
{
	if (*locale == '\0')
		return LOCALE_UTF8;

	if (strcmp(locale, "C") == 0 || strcmp(locale, "LOCALE_POSIX") == 0)
		return LOCALE_POSIX;
	if (strcmp(locale, "C.UTF-8") == 0)
		return LOCALE_UTF8;
	return LOCALE_INVALID;
}

static const char* locale_to_str(locale_t locale)
{
	if (locale == LOCALE_POSIX)
		return "C";
	if (locale == LOCALE_UTF8)
		return "C.UTF-8";
	ASSERT_NOT_REACHED();
}

struct lconv* localeconv(void)
{
	static lconv lconv;
	lconv.currency_symbol = const_cast<char*>("");
	lconv.decimal_point = const_cast<char*>(".");
	lconv.frac_digits = CHAR_MAX;
	lconv.grouping = const_cast<char*>("");
	lconv.int_curr_symbol = const_cast<char*>("");
	lconv.int_frac_digits = CHAR_MAX;
	lconv.int_n_cs_precedes = CHAR_MAX;
	lconv.int_n_sep_by_space = CHAR_MAX;
	lconv.int_n_sign_posn = CHAR_MAX;
	lconv.int_p_cs_precedes = CHAR_MAX;
	lconv.int_p_sep_by_space = CHAR_MAX;
	lconv.int_p_sign_posn = CHAR_MAX;
	lconv.mon_decimal_point = const_cast<char*>("");
	lconv.mon_grouping = const_cast<char*>("");
	lconv.mon_thousands_sep = const_cast<char*>("");
	lconv.negative_sign = const_cast<char*>("");
	lconv.n_cs_precedes = CHAR_MAX;
	lconv.n_sep_by_space = CHAR_MAX;
	lconv.n_sign_posn = CHAR_MAX;
	lconv.positive_sign = const_cast<char*>("");
	lconv.p_cs_precedes = CHAR_MAX;
	lconv.p_sep_by_space = CHAR_MAX;
	lconv.p_sign_posn = CHAR_MAX;
	lconv.thousands_sep = const_cast<char*>("");
	return &lconv;
}

char* setlocale(int category, const char* locale_str)
{
	static char s_locale_buffer[128];

	if (locale_str == nullptr)
	{
		switch (category)
		{
			case LC_COLLATE:
			case LC_CTYPE:
			case LC_MESSAGES:
			case LC_MONETARY:
			case LC_NUMERIC:
			case LC_TIME:
				strcpy(s_locale_buffer, locale_to_str(s_current_locales[category]));
				break;
			case LC_ALL:
				sprintf(s_locale_buffer, "%s;%s;%s;%s;%s;%s",
					locale_to_str(s_current_locales[0]),
					locale_to_str(s_current_locales[1]),
					locale_to_str(s_current_locales[2]),
					locale_to_str(s_current_locales[3]),
					locale_to_str(s_current_locales[4]),
					locale_to_str(s_current_locales[5])
				);
				break;
			default:
				return nullptr;
		}

		return s_locale_buffer;
	}

	locale_t locale = str_to_locale(locale_str);
	if (locale == LOCALE_INVALID)
		return nullptr;

	switch (category)
	{
		case LC_COLLATE:
		case LC_CTYPE:
		case LC_MESSAGES:
		case LC_MONETARY:
		case LC_NUMERIC:
		case LC_TIME:
			s_current_locales[category] = locale;
			break;
		case LC_ALL:
			for (auto& current : s_current_locales)
				current = locale;
			break;
		default:
			return nullptr;
	}

	strcpy(s_locale_buffer, locale_to_str(locale));
	return s_locale_buffer;
}

locale_t __getlocale(int category)
{
	switch (category)
	{
		case LC_COLLATE:
		case LC_CTYPE:
		case LC_MESSAGES:
		case LC_MONETARY:
		case LC_NUMERIC:
		case LC_TIME:
			return s_current_locales[category];
		default:
			return LOCALE_INVALID;
	}
}
