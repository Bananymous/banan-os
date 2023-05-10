#pragma once

#include <sys/cdefs.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/setjmp.h.html

__BEGIN_DECLS

typedef int* jmp_buf;
typedef int* sigjmp_buf;

void	_longjmp(jmp_buf, int);
void	longjmp(jmp_buf, int);
void	siglongjmp(sigjmp_buf, int);
int		_setjmp(jmp_buf);
int		setjmp(jmp_buf);
int		sigsetjmp(sigjmp_buf, int);

__END_DECLS
