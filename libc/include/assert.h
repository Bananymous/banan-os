#ifndef __ASSERT_H
#define _ASSERT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/assert.h.html

#include <sys/cdefs.h>

#ifdef NDEBUG
	#define assert(ignore) ((void)0)
#else
	#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

__BEGIN_DECLS

[[noreturn]] void __assert_fail(const char*, const char*, int, const char*);

__END_DECLS

#endif
