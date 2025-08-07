#include <BAN/Assert.h>
#include <BAN/UTF8.h>

#include <errno.h>
#include <locale.h>
#include <wchar.h>

wint_t btowc(int c)
{
	if (c == 0 || c > 0x7F)
		return WEOF;
	return c;
}

int wctob(wint_t c)
{
	if (c > 0x7F)
		return WEOF;
	return c;
}

int wcwidth(wchar_t wc)
{
	return wc != '\0';
}

wchar_t* wcstok(wchar_t* __restrict, const wchar_t* __restrict, wchar_t** __restrict) { ASSERT_NOT_REACHED(); }
long wcstol(const wchar_t* __restrict, wchar_t** __restrict, int) { ASSERT_NOT_REACHED(); }
int swprintf(wchar_t* __restrict, size_t, const wchar_t* __restrict, ...) { ASSERT_NOT_REACHED(); }

size_t wcrtomb(char* __restrict s, wchar_t ws, mbstate_t* __restrict ps)
{
	(void)ps;

	// ws == '\0' doesn't seem to apply to UTF8/ASCII?

	if (s == nullptr)
		return 1;

	switch (__getlocale(LC_CTYPE))
	{
		case locale_t::LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case locale_t::LOCALE_POSIX:
			if (ws > 0x7F)
				break;
			*s = ws;
			return 1;
		case locale_t::LOCALE_UTF8:
			if (!BAN::UTF8::from_codepoints(&ws, 1, s))
				break;
			return BAN::UTF8::byte_length(s[0]);
	}

	errno = EILSEQ;
	return -1;
}

size_t mbrtowc(wchar_t* __restrict pwc, const char* __restrict s, size_t n, mbstate_t* __restrict ps)
{
	(void)ps;

	if (s == nullptr)
		return 0;

	const auto locale = __getlocale(LC_CTYPE);

	size_t bytes = -1;
	switch (locale)
	{
		case locale_t::LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case locale_t::LOCALE_POSIX:
			bytes = 1;
			break;
		case locale_t::LOCALE_UTF8:
			if (auto b = BAN::UTF8::byte_length(s[0]); b != BAN::UTF8::invalid)
				bytes = b;
			break;
	}

	if (bytes == static_cast<size_t>(-1))
	{
		errno = EILSEQ;
		return -1;
	}

	if (n < bytes)
		return -1;

	wchar_t codepoint = WEOF;
	switch (locale)
	{
		case locale_t::LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case locale_t::LOCALE_POSIX:
			codepoint = s[0];
			break;
		case locale_t::LOCALE_UTF8:
			if (auto cp = BAN::UTF8::to_codepoint(s); cp != BAN::UTF8::invalid)
				codepoint = cp;
			break;
	}

	if (codepoint == WEOF)
	{
		errno = EILSEQ;
		return -1;
	}

	if (pwc != nullptr)
		*pwc = codepoint;

	if (codepoint == 0)
		return 0;

	return bytes;
}

int wcscoll(const wchar_t* ws1, const wchar_t* ws2)
{
	return wcscoll_l(ws1, ws2, __getlocale(LC_COLLATE));
}

int wcscoll_l(const wchar_t* ws1, const wchar_t* ws2, locale_t locale)
{
	(void)locale;
	// TODO: this isn't really correct :D
	return wcscmp(ws1, ws2);
}

size_t wcsxfrm(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n)
{
	return wcsxfrm_l(ws1, ws2, n, __getlocale(LC_COLLATE));
}

size_t wcsxfrm_l(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n, locale_t locale)
{
	(void)locale;
	// TODO: this isn't really correct :D
	wcsncpy(ws1, ws2, n);
	return wcslen(ws2);
}

size_t wcsftime(wchar_t* __restrict wcs, size_t maxsize, const wchar_t* __restrict format, const struct tm* __restrict timeptr)
{
	(void)wcs;
	(void)maxsize;
	(void)format;
	(void)timeptr;
	fprintf(stddbg, "TODO: wcsftime");
	return 0;
}

int wcscmp(const wchar_t* ws1, const wchar_t* ws2)
{
	for (; *ws1 && *ws2; ws1++, ws2++)
		if (*ws1 != *ws2)
			break;
	return *ws1 - *ws2;
}

int wcsncmp(const wchar_t* ws1, const wchar_t* ws2, size_t n)
{
	if (n == 0)
		return 0;
	for (; --n && *ws1 && *ws2; ws1++, ws2++)
		if (*ws1 != *ws2)
			break;
	return *ws1 - *ws2;
}

size_t wcslen(const wchar_t* ws)
{
	size_t len = 0;
	for (; ws[len]; len++)
		continue;
	return len;
}

size_t wcsnlen(const wchar_t* ws, size_t maxlen)
{
	size_t len = 0;
	for (; ws[len] && len < maxlen; len++)
		continue;
	return len;
}

wchar_t* wcpcpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2)
{
	while (*ws2)
		*ws1++ = *ws2++;
	*ws1 = L'\0';
	return ws1;
}

wchar_t* wcscpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2)
{
	wcpcpy(ws1, ws2);
	return ws1;
}

wchar_t* wcpncpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n)
{
	size_t i = 0;
	for (; ws2[i] && i < n; i++)
		ws1[i] = ws2[i];
	for (; i < n; i++)
		ws1[i] = L'\0';
	return &ws1[i];
}

wchar_t* wcsncpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n)
{
	wcpncpy(ws1, ws2, n);
	return ws1;
}

wchar_t* wcscat(wchar_t* __restrict ws1, const wchar_t* __restrict ws2)
{
	wcscpy(ws1 + wcslen(ws1), ws2);
	return ws1;
}

wchar_t* wcsncat(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n)
{
	size_t i = 0;
	for (; ws2[i] && i < n; i++)
		ws1[i] = ws2[i];
	ws1[i] = L'\0';
	return ws1;
}

static size_t wcsspn_impl(const wchar_t* ws1, const wchar_t* ws2, bool accept)
{
	size_t len = 0;
	for (; ws1[len]; len++)
	{
		bool found = false;
		for (size_t i = 0; ws2[i] && !found; i++)
			if (ws1[len] == ws2[i])
				found = true;
		if (found != accept)
			break;
	}
	return len;
}

size_t wcsspn(const wchar_t* ws1, const wchar_t* ws2)
{
	return wcsspn_impl(ws1, ws2, true);
}

size_t wcscspn(const wchar_t* ws1, const wchar_t* ws2)
{
	return wcsspn_impl(ws1, ws2, false);
}

wchar_t* wcschr(const wchar_t* ws, wchar_t wc)
{
	if (wc == L'\0')
		return const_cast<wchar_t*>(ws + wcslen(ws));
	for (size_t i = 0; ws[i]; i++)
		if (ws[i] == wc)
			return const_cast<wchar_t*>(&ws[i]);
	return nullptr;
}

wchar_t* wcsrchr(const wchar_t* ws, wchar_t wc)
{
	wchar_t* result = nullptr;
	for (size_t i = 0; ws[i]; i++)
		if (ws[i] == wc)
			result = const_cast<wchar_t*>(&ws[i]);
	return result;
}

wchar_t* wcsdup(const wchar_t* string)
{
	const size_t len = wcslen(string);
	wchar_t* result = static_cast<wchar_t*>(malloc((len + 1) * sizeof(wchar_t)));
	if (result == nullptr)
		return nullptr;
	wmemcpy(result, string, len + 1);
	return result;
}

wchar_t* wmemchr(const wchar_t* ws, wchar_t wc, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (ws[i] == wc)
			return const_cast<wchar_t*>(&ws[i]);
	return nullptr;
}

int wmemcmp(const wchar_t* ws1, const wchar_t* ws2, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (ws1[i] != ws2[i])
			return ws1[i] - ws2[i];
	return 0;
}

wchar_t* wmemcpy(wchar_t* __restrict ws1, const wchar_t* __restrict ws2, size_t n)
{
	for (size_t i = 0; i < n; i++)
		ws1[i] = ws2[i];
	return ws1;
}

wchar_t* wmemmove(wchar_t* ws1, const wchar_t* ws2, size_t n)
{
	if (ws1 < ws2)
	{
		for (size_t i = 0; i < n; i++)
			ws1[i] = ws2[i];
	}
	else
	{
		for (size_t i = 1; i <= n; i++)
			ws1[n - i] = ws2[n - i];
	}
	return ws1;
}

wchar_t* wmemset(wchar_t* ws, wchar_t wc, size_t n)
{
	for (size_t i = 0; i < n; i++)
		ws[i] = wc;
	return ws;
}

// FIXME: actually support multibyte :D

wint_t towlower(wint_t wc)
{
	return tolower(wc);
}

wint_t towupper(wint_t wc)
{
	return toupper(wc);
}

#define DEFINE_ISW(class) \
	int isw##class(wint_t wc) { \
		return is##class(wc); \
	}
DEFINE_ISW(alnum);
DEFINE_ISW(alpha);
DEFINE_ISW(blank);
DEFINE_ISW(cntrl);
DEFINE_ISW(digit);
DEFINE_ISW(graph);
DEFINE_ISW(lower);
DEFINE_ISW(print);
DEFINE_ISW(punct);
DEFINE_ISW(space);
DEFINE_ISW(upper);
DEFINE_ISW(xdigit);
#undef DEFINE_ISW

typedef enum {
	_alnum = 1,
	_alpha,
	_blank,
	_cntrl,
	_digit,
	_graph,
	_lower,
	_print,
	_punct,
	_space,
	_upper,
	_xdigit,
} wctype_values;

wctype_t wctype(const char* property)
{
#define CHECK_PROPERTY(class) \
	if (strcmp(property, #class) == 0) \
		return _##class
	CHECK_PROPERTY(alnum);
	CHECK_PROPERTY(alpha);
	CHECK_PROPERTY(blank);
	CHECK_PROPERTY(cntrl);
	CHECK_PROPERTY(digit);
	CHECK_PROPERTY(graph);
	CHECK_PROPERTY(lower);
	CHECK_PROPERTY(print);
	CHECK_PROPERTY(punct);
	CHECK_PROPERTY(space);
	CHECK_PROPERTY(upper);
	CHECK_PROPERTY(xdigit);
#undef CHECK_TYPE
	return 0;
}

int iswctype(wint_t wc, wctype_t charclass)
{
	switch (charclass)
	{
#define CLASS_CASE(class) \
		case _##class: \
			return is##class(wc)
		CLASS_CASE(alnum);
		CLASS_CASE(alpha);
		CLASS_CASE(blank);
		CLASS_CASE(cntrl);
		CLASS_CASE(digit);
		CLASS_CASE(graph);
		CLASS_CASE(lower);
		CLASS_CASE(print);
		CLASS_CASE(punct);
		CLASS_CASE(space);
		CLASS_CASE(upper);
		CLASS_CASE(xdigit);
#undef CLASS_CASE
	}
	return 0;
}
