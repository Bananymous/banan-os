#ifndef _WCTYPE_H
#define _WCTYPE_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/wctype.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include <bits/types/locale_t.h>

typedef int* wctrans_t;

int			iswalnum(wint_t wc);
int			iswalnum_l(wint_t wc, locale_t locale);
int			iswalpha(wint_t wc);
int			iswalpha_l(wint_t wc, locale_t locale);
int			iswblank(wint_t wc);
int			iswblank_l(wint_t wc, locale_t locale);
int			iswcntrl(wint_t wc);
int			iswcntrl_l(wint_t wc, locale_t locale);
int			iswctype(wint_t wc, wctype_t);
int			iswctype_l(wint_t wc, wctype_t, locale_t locale);
int			iswdigit(wint_t wc);
int			iswdigit_l(wint_t wc, locale_t locale);
int			iswgraph(wint_t wc);
int			iswgraph_l(wint_t wc, locale_t locale);
int			iswlower(wint_t wc);
int			iswlower_l(wint_t wc, locale_t locale);
int			iswprint(wint_t wc);
int			iswprint_l(wint_t wc, locale_t locale);
int			iswpunct(wint_t wc);
int			iswpunct_l(wint_t wc, locale_t locale);
int			iswspace(wint_t wc);
int			iswspace_l(wint_t wc, locale_t locale);
int			iswupper(wint_t wc);
int			iswupper_l(wint_t wc, locale_t locale);
int			iswxdigit(wint_t wc);
int			iswxdigit_l(wint_t wc, locale_t locale);
wint_t		towctrans(wint_t wc, wctrans_t);
wint_t		towctrans_l(wint_t wc, wctrans_t, locale_t locale);
wint_t		towlower(wint_t wc);
wint_t		towlower_l(wint_t wc, locale_t locale);
wint_t		towupper(wint_t wc);
wint_t		towupper_l(wint_t wc, locale_t locale);
wctrans_t	wctrans(const char* charclass);
wctrans_t	wctrans_l(const char* charclass, locale_t locale);
wctype_t	wctype(const char* property);
wctype_t	wctype_l(const char* property, locale_t locale);

__END_DECLS

#endif
