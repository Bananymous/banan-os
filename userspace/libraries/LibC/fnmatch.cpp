#include <fnmatch.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int fnmatch_impl(const char* pattern, const char* string, int flags, bool leading)
{
	while (*pattern)
	{
		if ((flags & FNM_PERIOD) && leading && *string == '.' && *pattern != '.')
			return FNM_NOMATCH;
		leading = false;

		switch (*pattern)
		{
			case '*':
			{
				const char* ptr = strchrnul(string, (flags & FNM_PATHNAME) ? '/' : '0');
				while (ptr >= string)
					if (fnmatch_impl(pattern + 1, ptr--, flags, false) == 0)
						return 0;
				return FNM_NOMATCH;
			}
			case '[':
			{
				if (strchr(pattern, ']') == nullptr)
					break;
				pattern++;

				const bool negate = (*pattern == '!');
				if (negate)
					pattern++;

				uint8_t ch;
				uint32_t bitmap[0x100 / 8] {};
				while ((ch = *pattern++) != ']')
					bitmap[ch / 32] |= 1 << (ch % 32);

				ch = *string++;
				if (!!(bitmap[ch / 32] & (1 << (ch % 32))) == negate)
					return FNM_NOMATCH;

				continue;
			}
			case '?':
			{
				if (*string == '\0')
					return FNM_NOMATCH;
				if ((flags & FNM_PATHNAME) && *string == '/')
					return FNM_NOMATCH;
				pattern++;
				string++;
				continue;
			}
			case '\\':
			{
				if (!(flags & FNM_NOESCAPE))
					pattern++;
				break;
			}
		}

		if (*pattern == '\0')
			break;

		if (*pattern != *string)
			return FNM_NOMATCH;
		if ((flags & FNM_PATHNAME) && *string == '/')
			leading = true;
		pattern++;
		string++;
	}

	return *string ? FNM_NOMATCH : 0;
}

int fnmatch(const char* pattern, const char* string, int flags)
{
	return fnmatch_impl(pattern, string, flags, true);
}
