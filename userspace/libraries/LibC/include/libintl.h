#ifndef _LIBINTL_H
#define _LIBINTL_H 1

// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/libintl.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/locale_t.h>

#include <limits.h>

char* bindtextdomain(const char*, const char*);
char* bind_textdomain_codeset(const char*, const char*);
char* dcgettext(const char*, const char*, int);
char* dcgettext_l(const char*, const char*, int, locale_t);
char* dcngettext(const char*, const char*, const char*, unsigned long int, int);
char* dcngettext_l(const char*, const char*, const char*, unsigned long int, int, locale_t);
char* dgettext(const char*, const char*);
char* dgettext_l(const char*, const char*, locale_t);
char* dngettext(const char*, const char*, const char*, unsigned long int);
char* dngettext_l(const char*, const char*, const char*, unsigned long int, locale_t);
char* gettext(const char*);
char* gettext_l(const char*, locale_t);
char* ngettext(const char*, const char*, unsigned long int);
char* ngettext_l(const char*, const char*, unsigned long int, locale_t);
char* textdomain(const char*);

__END_DECLS

#endif
