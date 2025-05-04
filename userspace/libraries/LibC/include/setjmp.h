#ifndef _SETJMP_H
#define _SETJMP_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/setjmp.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#if defined(__x86_64__)
#define _JMP_BUF_REGS 8 // rsp, rip, rbx, rbp, r12-r15
#elif defined(__i686__)
#define _JMP_BUF_REGS 6 // esp, eip, ebx, ebp, edi, esi
#endif

typedef long jmp_buf[_JMP_BUF_REGS];
typedef long sigjmp_buf[_JMP_BUF_REGS + 1 + (sizeof(long long) / sizeof(long))];

#define _longjmp longjmp
void longjmp(jmp_buf env, int val);
void siglongjmp(sigjmp_buf env, int val);

#define _setjmp setjmp
int setjmp(jmp_buf env);
int sigsetjmp(sigjmp_buf env, int savemask);

__END_DECLS

#endif
