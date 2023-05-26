#ifndef _SETJMP_H
#define _SETJMP_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/setjmp.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int jmp_buf[1];
typedef int sigjmp_buf[1];

void	longjmp(jmp_buf env, int val);
void	siglongjmp(sigjmp_buf env, int val);
int		setjmp(jmp_buf env);
int		sigsetjmp(sigjmp_buf env, int savemask);

__END_DECLS

#endif
