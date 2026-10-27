#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

static const char* s_log_ident = nullptr;
static FILE* s_log_file = nullptr;
static int s_log_mask = LOG_UPTO(LOG_DEBUG);

void openlog(const char* ident, int logopt, int facility)
{
	(void)facility;

	if ((logopt & LOG_NDELAY) && s_log_file == nullptr)
		s_log_file = fopen("/dev/debug", "w");

	s_log_ident = ident;
}

int setlogmask(int maskpri)
{
	const int old = s_log_mask;
	if (maskpri != 0)
		s_log_mask = maskpri;
	return old;
}

void closelog()
{
	if (s_log_file == nullptr)
		return;
	fclose(s_log_file);
	s_log_file = nullptr;
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
	if (!(LOG_MASK(priority) & s_log_mask))
		return;

	if (!s_log_file && !(s_log_file = fopen("/dev/debug", "w")))
		return;

	if (s_log_ident)
		fprintf(s_log_file, "%s: ", s_log_ident);

	vfprintf(s_log_file, format, ap);

	const size_t format_len = strlen(format);
	if (format_len && format[format_len - 1] != '\n')
		fputc('\n', s_log_file);
}
