#ifndef _WCHAR_H
#define _WCHAR_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/wchar.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <bits/types/locale_t.h>

typedef int mbstate_t;

typedef int wctype_t;

typedef __WINT_TYPE__ wint_t;

#define WCHAR_MIN	__WCHAR_MIN__
#define WCHAR_MAX	__WCHAR_MAX__
#define WEOF		((wchar_t)-1)

wint_t				btowc(int c);
wint_t				fgetwc(FILE* stream);
wchar_t*			fgetws(wchar_t* __restrict ws, int n, FILE* __restrict stream);
wint_t				fputwc(wchar_t wc, FILE* stream);
int					fputws(const wchar_t* __restrict ws, FILE* __restrict stream);
int					fwide(FILE* stream, int mode);
int					fwprintf(FILE* __restrict stream, const wchar_t* __restrict format, ...);
int					fwscanf(FILE* __restrict stream, const wchar_t* __restrict format, ...);
wint_t				getwc(FILE* stream);
wint_t				getwchar(void);
int					iswalnum(wint_t wc);
int					iswalpha(wint_t wc);
int					iswcntrl(wint_t wc);
int					iswctype(wint_t wc, wctype_t charclass);
int					iswdigit(wint_t wc);
int					iswgraph(wint_t wc);
int					iswlower(wint_t wc);
int					iswprint(wint_t wc);
int					iswpunct(wint_t wc);
int					iswspace(wint_t wc);
int					iswupper(wint_t wc);
int					iswxdigit(wint_t wc);
size_t				mbrlen(const char* __restrict s, size_t n, mbstate_t* __restrict ps);
size_t				mbrtowc(wchar_t* __restrict pwc, const char* __restrict s, size_t n, mbstate_t* __restrict ps);
int					mbsinit(const mbstate_t* ps);
size_t				mbsnrtowcs(wchar_t* __restrict dst, const char** __restrict src, size_t nmc, size_t len, mbstate_t* __restrict ps);
size_t				mbsrtowcs(wchar_t* __restrict dst, const char** __restrict src, size_t len, mbstate_t* __restrict ps);
FILE*				open_wmemstream(wchar_t** bufp, size_t* sizep);
wint_t				putwc(wchar_t wc, FILE* stream);
wint_t				putwchar(wchar_t wc);
int					swprintf(wchar_t* __restrict ws, size_t n, const wchar_t* __restrict format, ...);
int					swscanf(const wchar_t* __restrict ws, const wchar_t* __restrict format, ...);
wint_t				towlower(wint_t wc);
wint_t				towupper(wint_t wc);
wint_t				ungetwc(wint_t wc, FILE* stream);
int					vfwprintf(FILE* __restrict stream, const wchar_t* __restrict format, va_list arg);
int					vfwscanf(FILE* __restrict stream, const wchar_t* __restrict format, va_list arg);
int					vswprintf(wchar_t* __restrict ws, size_t n, const wchar_t* __restrict format, va_list arg);
int					vswscanf(const wchar_t* __restrict ws, const wchar_t* __restrict format, va_list arg);
int					vwprintf(const wchar_t* __restrict format, va_list arg);
int					vwscanf(const wchar_t* __restrict format, va_list arg);
wchar_t*			wcpcpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2);
wchar_t*			wcpncpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n);
size_t				wcrtomb(char* __restrict s, wchar_t ws, mbstate_t* __restrict ps);
int					wcscasecmp(const wchar_t* ws1, const wchar_t* ws2);
int					wcscasecmp_l(const wchar_t* ws1, const wchar_t* ws2, locale_t locale);
wchar_t*			wcscat(wchar_t* __restrict ws1, const wchar_t* __restrict ws2);
wchar_t*			wcschr(const wchar_t* ws, wchar_t wc);
int					wcscmp(const wchar_t* ws1, const wchar_t* ws2);
int					wcscoll(const wchar_t* ws1, const wchar_t* ws2);
int					wcscoll_l(const wchar_t* ws1, const wchar_t* ws2, locale_t locale);
wchar_t*			wcscpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2);
size_t				wcscspn(const wchar_t* ws1, const wchar_t* ws2);
wchar_t*			wcsdup(const wchar_t* string);
size_t				wcsftime(wchar_t* __restrict wcs, size_t maxsize, const wchar_t* __restrict format, const struct tm* __restrict timeptr);
size_t				wcslen(const wchar_t* ws);
int					wcsncasecmp(const wchar_t* ws1, const wchar_t* ws2, size_t n);
int					wcsncasecmp_l(const wchar_t* ws1, const wchar_t* ws2, size_t n, locale_t locale);
wchar_t*			wcsncat(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n);
int					wcsncmp(const wchar_t* ws1, const wchar_t* ws2, size_t n);
wchar_t*			wcsncpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n);
size_t				wcsnlen(const wchar_t* ws, size_t maxlen);
size_t				wcsnrtombs(char* __restrict dst, const wchar_t** __restrict src, size_t nwc, size_t len, mbstate_t* __restrict ps);
wchar_t*			wcspbrk(const wchar_t* ws1, const wchar_t* ws2);
wchar_t*			wcsrchr(const wchar_t* ws, wchar_t wc);
size_t				wcsrtombs(char* __restrict dst, const wchar_t** __restrict src, size_t len, mbstate_t* __restrict ps);
size_t				wcsspn(const wchar_t* ws1, const wchar_t* ws2);
wchar_t*			wcsstr(const wchar_t* __restrict ws1, const wchar_t* __restrict ws2);
double				wcstod(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);
float				wcstof(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);
wchar_t*			wcstok(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, wchar_t** __restrict ptr);
long				wcstol(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
long double			wcstold(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);
long long			wcstoll(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
unsigned long		wcstoul(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
unsigned long long	wcstoull(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
int					wcswidth(const wchar_t* pwcs, size_t n);
size_t				wcsxfrm(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n);
size_t				wcsxfrm_l(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n, locale_t locale);
int					wctob(wint_t c);
wctype_t			wctype(const char* property);
int					wcwidth(wchar_t wc);
wchar_t*			wmemchr(const wchar_t* ws, wchar_t wc, size_t n);
int					wmemcmp(const wchar_t* ws1, const wchar_t* ws2, size_t n);
wchar_t*			wmemcpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n);
wchar_t*			wmemmove(wchar_t* ws1, const wchar_t* ws2, size_t n);
wchar_t*			wmemset(wchar_t* ws, wchar_t wc, size_t n);
int					wprintf(const wchar_t* __restrict format, ...);
int					wscanf(const wchar_t* __restrict format, ...);

__END_DECLS

#endif
