#include <stdio.h>
#include <string.h>
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

void closelog()
{
	fclose(s_log_file);
	s_log_file = nullptr;
	s_ident = nullptr;
}

void syslog(int priority, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsyslog(priority, format, args);
	va_end(args);
}

void vsyslog(int priority, const char* format, va_list ap)
{
	(void)priority;

	if (s_ident)
		fprintf(s_log_file, "%s: ", s_ident);

	vfprintf(s_log_file, format, ap);

	const size_t format_len = strlen(format);
	if (format_len && format[format_len - 1] != '\n')
		fputc('\n', s_log_file);
}
