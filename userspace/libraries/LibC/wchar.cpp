#include <BAN/Assert.h>

#include <wchar.h>

size_t mbrtowc(wchar_t* __restrict, const char* __restrict, size_t, mbstate_t* __restrict)
{
	ASSERT_NOT_REACHED();
}
