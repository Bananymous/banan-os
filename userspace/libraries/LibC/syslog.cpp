#include <BAN/Assert.h>

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

static const char* s_ident = nullptr;

void openlog(const char* ident, int option, int facility)
{
	(void)option;
	(void)facility;
	s_ident = ident;
}

void syslog(int priority, const char* format, ...)
{
	(void)priority;
	if (s_ident)
		fprintf(stddbg, "%s", s_ident);
	va_list args;
	va_start(args, format);
	vfprintf(stddbg, format, args);
	va_end(args);
}

void closelog()
{
	s_ident = nullptr;
}
