#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void err(int eval, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verr(eval, fmt, args);
}

void errx(int eval, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verrx(eval, fmt, args);
}

void warn(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vwarn(fmt, args);
	va_end(args);
}

void warnx(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vwarnx(fmt, args);
	va_end(args);
}

void verr(int eval, const char* fmt, va_list args)
{
	vwarn(fmt, args);
	exit(eval);
}

void verrx(int eval, const char* fmt, va_list args)
{
	vwarnx(fmt, args);
	exit(eval);
}

static void _vwarn_impl(const char* fmt, va_list args)
{
	fprintf(stderr, "%s", getprogname() ?: "<prog>");
	if (fmt == nullptr)
		return;
	fprintf(stderr, ": ");
	vfprintf(stderr, fmt, args);
}

void vwarn(const char* fmt, va_list args)
{
	const int saved_errno = errno;
	_vwarn_impl(fmt, args);
	fprintf(stderr, ": %s\n", strerror(saved_errno));
}

void vwarnx(const char* fmt, va_list args)
{
	_vwarn_impl(fmt, args);
	fprintf(stderr, "\n");
}
