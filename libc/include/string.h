#pragma once

#include <locale.h>
#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void*	memccpy(void*, const void*, int, size_t);
void*	memchr(const void*, int, size_t);
int		memcmp(const void*, const void*, size_t);
void*	memcpy(void*, const void*, size_t);
void*	memmove(void*, const void*, size_t);
void*	memset(void*, int, size_t);
char*	stpcpy(char*, const char*);
char*	stpncpy(char*, const char*, size_t);
char*	strcat(char*, const char*);
char*	strchr(const char*, int);
int		strcmp(const char*, const char*);
int		strcoll(const char*, const char*);
int		strcoll_l(const char*, const char*, locale_t);
char*	strcpy(char*, const char*);
size_t	strcspn(const char*, const char*);
char*	strdup(const char*);
char*	strerror(int);
char*	strerror_l(int, locale_t);
int		strerror_r(int, char*, size_t);
size_t	strlen(const char*);
char*	strncat(char*, const char*, size_t);
int		strncmp(const char*, const char*, size_t);
char*	strncpy(char*, const char*, size_t);
char*	strndup(const char*, size_t);
size_t	strnlen(const char*, size_t);
char*	strpbrk(const char*, const char*);
char*	strrchr(const char*, int);
char*	strsignal(int);
size_t	strspn(const char*, const char*);
char*	strstr(const char*, const char*);
char*	strtok(char*, const char*);
char*	strtok_r(char*, const char*, char**);
size_t	strxfrm(char*, const char*, size_t);
size_t	strxfrm_l(char*, const char*, size_t, locale_t);

const char* strerrorname_np(int);
const char* strerrordesc_np(int);

__END_DECLS