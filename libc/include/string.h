#ifndef _STRING_H
#define _STRING_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/string.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stddef.h>
#include <bits/types/locale_t.h>

void*	memccpy(void* __restrict s1, const void* __restrict s2, int c, size_t n);
void*	memchr(const void* s, int c, size_t n);
int		memcmp(const void* s1, const void* s2, size_t n);
void*	memcpy(void* __restrict s1, const void* __restrict s2, size_t n);
void*	memmove(void* s1, const void* s2, size_t n);
void*	memset(void* s, int c, size_t n);
char*	stpcpy(char* __restrict s1, const char* __restrict s2);
char*	stpncpy(char* __restrict s1, const char* __restrict s2, size_t n);
char*	strcat(char* __restrict s1, const char* __restrict s2);
char*	strchr(const char* s, int c);
char*	strchrnul(const char* s, int c);
int		strcmp(const char* s1, const char* s2);
int		strcoll(const char* s1, const char* s2);
int		strcoll_l(const char* s1, const char* s2, locale_t locale);
char*	strcpy(char* __restrict s1, const char* __restrict s2);
size_t	strcspn(const char* , const char* );
char*	strdup(const char* s);
char*	strerror(int errnum);
char*	strerror_l(int errnum, locale_t locale);
int		strerror_r(int errnum, char* strerrbuf, size_t buflen);
size_t	strlen(const char* s);
char*	strncat(char* __restrict s1, const char* __restrict s2, size_t n);
int		strncmp(const char* s1, const char* s2, size_t n);
char*	strncpy(char* __restrict s1, const char* __restrict s2, size_t n);
char*	strndup(const char* s, size_t n);
size_t	strnlen(const char* s, size_t maxlen);
char*	strpbrk(const char* s1, const char* s2);
char*	strrchr(const char* s, int c);
char*	strsignal(int signum);
size_t	strspn(const char* s1, const char* s2);
char*	strstr(const char* s1, const char* s2);
char*	strtok(char* __restrict s, const char* __restrict sep);
char*	strtok_r(char* __restrict s, const char* __restrict sep, char** __restrict state);
size_t	strxfrm(char* __restrict s1, const char* __restrict s2, size_t n);
size_t	strxfrm_l(char* __restrict s1, const char* __restrict s2, size_t n, locale_t locale);

const char* strerrorname_np(int error);
const char* strerrordesc_np(int error);


__END_DECLS

#endif
