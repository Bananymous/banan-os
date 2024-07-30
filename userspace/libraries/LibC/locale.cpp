#include <locale.h>
#include <string.h>

// FIXME: Actually support locales
char* setlocale(int category, const char* locale)
{
	(void)category;

	static char s_locale[] = "C";
	if (locale == nullptr)
		return s_locale;
	if (strcmp(locale, "") == 0 || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
		return s_locale;
	return nullptr;
}
