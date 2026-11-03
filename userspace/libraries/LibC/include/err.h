#ifndef _ERR_H
#define _ERR_H 1

// https://man7.org/linux/man-pages/man3/err.3.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need___va_list
#include <stdarg.h>

void err(int eval, const char* fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void errx(int eval, const char* fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

void warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void warnx(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void verr(int eval, const char* fmt, __gnuc_va_list args) __attribute__((noreturn));
void verrx(int eval, const char* fmt, __gnuc_va_list args) __attribute__((noreturn));

void vwarn(const char* fmt, __gnuc_va_list args);
void vwarnx(const char* fmt, __gnuc_va_list args);

__END_DECLS

#endif
