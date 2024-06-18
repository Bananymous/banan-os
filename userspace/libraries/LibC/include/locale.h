#ifndef _LOCALE_H
#define _LOCALE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/locale.h.html

#include <sys/cdefs.h>

#include <bits/types/locale_t.h>

#define __need_NULL
#include <stddef.h>

#define LC_COLLATE	0
#define LC_CTYPE	1
#define LC_MESSAGES	2
#define LC_MONETARY	3
#define LC_NUMERIC	4
#define LC_TIME 	5
#define LC_ALL		6

#define LC_COLLATE_MASK 	(1 << LC_COLLATE)
#define LC_CTYPE_MASK		(1 << LC_CTYPE)
#define LC_MESSAGES_MASK	(1 << LC_MESSAGES)
#define LC_MONETARY_MASK	(1 << LC_MONETARY)
#define LC_NUMERIC_MASK		(1 << LC_NUMERIC)
#define LC_TIME_MASK		(1 << LC_TIME)
#define LC_ALL_MASK   LC_COLLATE_MASK	\
					| LC_CTYPE_MASK		\
					| LC_MESSAGES_MASK	\
					| LC_MONETARY_MASK	\
					| LC_NUMERIC_MASK	\
					| LC_TIME_MASK

__BEGIN_DECLS

struct lconv
{
	char*	currency_symbol;
	char*	decimal_point;
	char	frac_digits;
	char*	grouping;
	char*	int_curr_symbol;
	char 	int_frac_digits;
	char 	int_n_cs_precedes;
	char 	int_n_sep_by_space;
	char 	int_n_sign_posn;
	char 	int_p_cs_precedes;
	char 	int_p_sep_by_space;
	char 	int_p_sign_posn;
	char*	mon_decimal_point;
	char*	mon_grouping;
	char*	mon_thousands_sep;
	char*	negative_sign;
	char 	n_cs_precedes;
	char 	n_sep_by_space;
	char 	n_sign_posn;
	char*	positive_sign;
	char 	p_cs_precedes;
	char 	p_sep_by_space;
	char 	p_sign_posn;
	char*	thousands_sep;
};

locale_t		duplocale(locale_t locobj);
void			freelocale(locale_t locobj);
struct lconv*	localeconv(void);
locale_t		newlocale(int category_mask, const char* locale, locale_t base);
char* 			setlocale(int category, const char* locale);
locale_t		uselocale(locale_t newloc);

__END_DECLS

#endif
