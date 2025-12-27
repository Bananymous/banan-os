#ifndef _ASSERT_H
#define _ASSERT_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/assert.h.html

#include <sys/cdefs.h>

#ifdef NDEBUG
	#define assert(ignore) ((void)0)
#else
	#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

#if !defined(__cplusplus) && __STDC_VERSION__ >= 201112L && __STDC_VERSION__ < 202311L
	#define static_assert _Static_assert
#endif

__BEGIN_DECLS

__attribute__((noreturn))
void __assert_fail(const char*, const char*, int, const char*);

__END_DECLS

#endif
