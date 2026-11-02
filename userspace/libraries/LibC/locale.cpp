#include <BAN/Assert.h>

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static __locale_t s_locale_c {
	.name = "C",
	.encoding = __ENC_ASCII,
};
static __locale_t s_locale_utf8 {
	.name = "en_US.UTF-8",
	.encoding = __ENC_UTF8,
};

static locale_t s_current_locales[LC_ALL] {
	&s_locale_c,
	&s_locale_c,
	&s_locale_c,
	&s_locale_c,
	&s_locale_c,
	&s_locale_c,
};
static_assert(LC_ALL == 6);

static locale_t str_to_locale(const char* locale)
{
	if (*locale == '\0')
		return &s_locale_utf8;

	if (strcmp(locale, s_locale_c.name) == 0 || strcmp(locale, "POSIX") == 0)
		return &s_locale_c;

	if (strcmp(locale, s_locale_utf8.name) == 0)
		return &s_locale_utf8;

	errno = EINVAL;
	return nullptr;
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
				strcpy(s_locale_buffer, s_current_locales[category]->name);
				break;
			case LC_ALL:
				sprintf(s_locale_buffer, "%s;%s;%s;%s;%s;%s",
					s_current_locales[0]->name,
					s_current_locales[1]->name,
					s_current_locales[2]->name,
					s_current_locales[3]->name,
					s_current_locales[4]->name,
					s_current_locales[5]->name
				);
				static_assert(LC_ALL == 6);
				break;
			default:
				return nullptr;
		}

		return s_locale_buffer;
	}

	locale_t locale = str_to_locale(locale_str);
	if (locale == nullptr)
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

	strcpy(s_locale_buffer, locale->name);
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
			return nullptr;
	}
}

#include <BAN/Debug.h>

locale_t duplocale(locale_t locobj)
{
	(void)locobj;
	dwarnln("TODO: duplocale");
	return nullptr;
}

locale_t newlocale(int category_mask, const char* locale, locale_t base)
{
	(void)category_mask;
	(void)locale;
	(void)base;
	dwarnln("TODO: newlocale");
	return nullptr;
}

void freelocale(locale_t locobj)
{
	(void)locobj;
	dwarnln("TODO: freelocale");
}

locale_t uselocale(locale_t newloc)
{
	(void)newloc;
	dwarnln("TODO: uselocale");
	return nullptr;
}
