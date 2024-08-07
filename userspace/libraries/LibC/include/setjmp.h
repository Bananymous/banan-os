#ifndef _SETJMP_H
#define _SETJMP_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/setjmp.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef long jmp_buf[2];
typedef long sigjmp_buf[2 + 1 + (sizeof(long long) / sizeof(long))];

#define _longjmp longjmp
void longjmp(jmp_buf env, int val);
void siglongjmp(sigjmp_buf env, int val);

#define _setjmp setjmp
int setjmp(jmp_buf env);
int sigsetjmp(sigjmp_buf env, int savemask);

__END_DECLS

#endif
