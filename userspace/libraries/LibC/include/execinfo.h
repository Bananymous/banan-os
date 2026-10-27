#ifndef _EXECINFO_H
#define _EXECINFO_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

int    backtrace(void* buffer[], int size);
char** backtrace_symbols(void* const buffer[], int size);
void   backtrace_symbols_fd(void* const buffer[], int size, int fd);

__END_DECLS

#endif
