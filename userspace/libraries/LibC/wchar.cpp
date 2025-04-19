#include <BAN/Assert.h>

#include <wchar.h>

size_t mbrtowc(wchar_t* __restrict, const char* __restrict, size_t, mbstate_t* __restrict)
{
	ASSERT_NOT_REACHED();
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
