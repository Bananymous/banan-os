#ifndef _FENV_H
#define _FENV_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/fenv.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

#define FE_INVALID   (1 << 0)
#define FE_DIVBYZERO (1 << 2)
#define FE_OVERFLOW  (1 << 3)
#define FE_UNDERFLOW (1 << 4)
#define FE_INEXACT   (1 << 5)
#define FE_ALL_EXCEPT (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

#define FE_TONEAREST  0
#define FE_DOWNWARD   1
#define FE_UPWARD     2
#define FE_TOWARDZERO 3

typedef struct {
	uint32_t control;
	uint32_t status;
	uint32_t __unused[5];
} __x87_fpu_t;

typedef struct {
	__x87_fpu_t x87_fpu;
	uint32_t mxcsr;
} fenv_t;

typedef uint8_t fexcept_t;

#define FE_DFL_ENV ((const fenv_t*)0x1)

int feclearexcept(int);
int fegetenv(fenv_t*);
int fegetexceptflag(fexcept_t*, int);
int fegetround(void);
int feholdexcept(fenv_t*);
int feraiseexcept(int);
int fesetenv(const fenv_t*);
int fesetexceptflag(const fexcept_t*, int);
int fesetround(int);
int fetestexcept(int);
int feupdateenv(const fenv_t*);

__END_DECLS

#endif
