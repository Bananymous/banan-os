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
