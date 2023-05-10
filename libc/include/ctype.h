#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

int isalnum(int);
int isalpha(int);
int isascii(int);
int isblank(int);
int iscntrl(int);
int isdigit(int);
int isgraph(int);
int islower(int);
int isprint(int);
int ispunct(int);
int isspace(int);
int isupper(int);
int isxdigit(int);

int toupper(int);
int tolower(int);

__END_DECLS