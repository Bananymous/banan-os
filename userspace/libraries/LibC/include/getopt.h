#ifndef _GETOPT_H
#define _GETOPT_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/getopt.h>

struct option
{
	const char* name;
	int has_arg;
	int* flag;
	int val;
};

#define no_argument 0
#define required_argument 1
#define optional_argument 2

int getopt_long(int argc, char* argv[], const char* optstring, const struct option* longopts, int* longindex);
int getopt_long_only(int argc, char* argv[], const char* optstring, const struct option* longopts, int* longindex);

__END_DECLS

#endif
