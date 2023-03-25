#pragma once

#include <sys/types.h>

__BEGIN_DECLS

struct DIR;

struct dirent
{
	ino_t d_ino;
	char d_name[];
};

__END_DECLS