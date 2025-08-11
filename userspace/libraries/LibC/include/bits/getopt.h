#ifndef _BITS_GETOPT_H
#define _BITS_GETOPT_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

int getopt(int argc, char* const argv[], const char* optstring);

extern char* optarg;
extern int   opterr, optind, optopt;

__END_DECLS

#endif
