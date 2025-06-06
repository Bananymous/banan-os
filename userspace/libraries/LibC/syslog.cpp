#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

static const char* s_ident = nullptr;

static FILE* s_log_file = nullptr;

void openlog(const char* ident, int option, int facility)
{
	if (s_log_file == nullptr)
		s_log_file = fopen("/dev/debug", "w");

	(void)option;
	(void)facility;
	s_ident = ident;
}

void syslog(int priority, const char* format, ...)
{
	(void)priority;
	if (s_ident)
		fprintf(s_log_file, "%s", s_ident);
	va_list args;
	va_start(args, format);
	vfprintf(s_log_file, format, args);
	va_end(args);
}

void closelog()
{
	fclose(s_log_file);
	s_log_file = nullptr;
	s_ident = nullptr;
}
